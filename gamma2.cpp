#include <ncurses.h>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <cmath>
using namespace std;

struct StanProcesu {
    int    od     = 0;
    int    do_    = 0;
    double wynik  = 0.0;
    bool   gotowy = false;
};


// WYŚWIETLANIE


void rysuj(int n, int p, const vector<StanProcesu>& stany, double wynik_koncowy, bool obliczono) {
    erase();
    mvprintw(0, 0, "STALA GAMMA EULERA   n = %d   procesy = %d", n, p);
    mvprintw(1, 0, "----------------------------------------------------");
    mvprintw(2, 0, "PROCES   ZAKRES         SUMA CZESCIOWA   STATUS");
    mvprintw(3, 0, "----------------------------------------------------");

    for (int i = 0; i < p; i++) {
        const auto& s = stany[i];
        if (s.gotowy)
            mvprintw(4 + i, 0, "P%-6d | %4d - %4d | %15.10f | gotowy",
                     i, s.od, s.do_, s.wynik);
        else
            mvprintw(4 + i, 0, "P%-6d | %4d - %4d | %15s | liczy...",
                     i, s.od, s.do_, "---");
    }

    int row = 5 + p;
    mvprintw(row++, 0, "----------------------------------------------------");
    if (obliczono)
        mvprintw(row++, 0, "WYNIK: gamma = %.10f   (dokladna: 0.5772156649)", wynik_koncowy);
    else
        mvprintw(row++, 0, "WYNIK: obliczanie...");

    mvprintw(row, 0, "Nacisnij dowolny klawisz aby wyjsc.");
    refresh();
}


// KLIENT


void f_klient(int id, int od, int do_, vector<array<int,2>>& pipes, int p) {
    // zamknij wszystkie końce do czytania klient tylko pisze
    for (int j = 0; j < p; j++) close(pipes[j][0]);
    // zamknij potoki innych klientów
    for (int j = 0; j < p; j++)
        if (j != id) close(pipes[j][1]);

    // oblicz fragment sumy
    double suma = 0.0;
    for (int k = od; k <= do_; k++)
        suma += 1.0 / k;

    // wyślij wynik do serwera przez potok
    write(pipes[id][1], &suma, sizeof(double));
    close(pipes[id][1]);
}

// SERWER

double f_serwer(int n, int p, vector<StanProcesu>& stany, vector<array<int,2>>& pipes) {
    // zamknij wszystkie końce do pisania serwer tylko czyta
    for (int i = 0; i < p; i++) close(pipes[i][1]);

    initscr(); curs_set(0); noecho();

    // czytaj wyniki od klientów po kolei
    double suma_calkowita = 0.0;
    for (int i = 0; i < p; i++) {
        double wynik = 0.0;
        read(pipes[i][0], &wynik, sizeof(double));
        close(pipes[i][0]);
        stany[i].wynik  = wynik;
        stany[i].gotowy = true;
        suma_calkowita += wynik;
        rysuj(n, p, stany, 0.0, false);
    }

    // odejmij logarytm — końcowy wynik
    double gamma = suma_calkowita - log(n);
    rysuj(n, p, stany, gamma, true);

    // czekaj na klawisz
    nodelay(stdscr, FALSE);
    getch();
    endwin();

    return gamma;
}


// PODZIAŁ ZAKRESÓW


void podziel_zakresy(int n, int p, vector<StanProcesu>& stany) {
    int bazowy  = n / p;
    int reszta  = n % p;
    int current = 1;
    for (int i = 0; i < p; i++) {
        int wielkosc   = bazowy + (i < reszta ? 1 : 0);
        stany[i].od    = current;
        stany[i].do_   = current + wielkosc - 1;
        current       += wielkosc;
    }
}

// MAIN

int main() {
    int n, p;
    cout << "Podaj n (liczba elementow sumy): "; cin >> n;
    cout << "Podaj p (liczba procesow): ";        cin >> p;
    if (n <= 0 || p <= 0) return 0;
    if (p > n) p = n;

    // oblicz zakresy dla każdego klienta
    vector<StanProcesu> stany(p);
    podziel_zakresy(n, p, stany);

    // stwórz potoki jeden na każdego klienta
    vector<array<int,2>> pipes(p);
    for (int i = 0; i < p; i++)
        pipe(pipes[i].data());

    // fork stwórz p procesów klientów
    for (int i = 0; i < p; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // KLIENT
            f_klient(i, stany[i].od, stany[i].do_, pipes, p);
            exit(0);
        }
    }

    // SERWER
    f_serwer(n, p, stany, pipes);

    // poczekaj na zakończenie wszystkich klientów
    for (int i = 0; i < p; i++) wait(nullptr);

    return 0;
}
