#include <ncurses.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <iostream>
#include <random>
using namespace std;

class Semafor {
    mutex              m;
    condition_variable cv;
    int                wartosc;
public:
    Semafor(int v) : wartosc(v) {}

    void czekaj() {
        unique_lock<mutex> lock(m);
        cv.wait(lock, [&]{ return wartosc > 0; });
        wartosc--;
    }

    void sygnal() {
        lock_guard<mutex> lock(m);
        wartosc++;
        cv.notify_one();
    }

    int get() {
        lock_guard<mutex> lock(m);
        return wartosc;
    }
};

struct StanPalacza {
    string status = "czeka na ubijacz";
};

class Kelner {
    Semafor    sem_ubijacze;
    Semafor    sem_zapalki;
    Semafor    sem_ekran;
    mutex      m_stany;
    int        n_ubijacze, n_zapalki;

public:
    vector<StanPalacza> stany;

    Kelner(int k, int l, int m)
        : sem_ubijacze(l), sem_zapalki(m), sem_ekran(1),
          n_ubijacze(l), n_zapalki(m) {
        stany.resize(k);
    }

    void ustaw_status(int id, const string& s) {
        lock_guard<mutex> lock(m_stany);
        stany[id].status = s;
    }

    void zadaj_ubijacz(int id) {
        ustaw_status(id, "czeka na ubijacz");
        sem_ubijacze.czekaj();
        ustaw_status(id, "ubija");
    }

    void zwroc_ubijacz(int id) {
        sem_ubijacze.sygnal();
    }

    void zadaj_zapalki(int id) {
        ustaw_status(id, "czeka na zapalki");
        sem_zapalki.czekaj();
        ustaw_status(id, "zapala fajke");
    }

    void zwroc_zapalki(int id) {
        sem_zapalki.sygnal();
        ustaw_status(id, "pali");
    }

    void rysuj(int k) {
        sem_ekran.czekaj();
        erase();
        mvprintw(0, 0, "UBIJACZE: %d/%d dostepnych   ZAPALKI: %d/%d dostepnych",
                 sem_ubijacze.get(), n_ubijacze,
                 sem_zapalki.get(),  n_zapalki);
        mvprintw(1, 0, "----------------------------------------------------");
        mvprintw(2, 0, "PALACZ     STATUS");
        mvprintw(3, 0, "----------------------------");
        {
            lock_guard<mutex> lock(m_stany);
            for (int i = 0; i < k; i++)
                mvprintw(4 + i, 0, "P%-8d | %s", i, stany[i].status.c_str());
        }
        mvprintw(5 + k, 0, "Nacisnij Ctrl+C aby wyjsc.");
        refresh();
        sem_ekran.sygnal();
    }
};

void f_palacz(int id, Kelner& k) {
    random_device rd; mt19937 g(rd());
    while (true) {
        k.zadaj_ubijacz(id);
        this_thread::sleep_for(chrono::milliseconds(500 + g() % 1000));
        k.zwroc_ubijacz(id);

        k.zadaj_zapalki(id);
        this_thread::sleep_for(chrono::milliseconds(300 + g() % 500));
        k.zwroc_zapalki(id);

        this_thread::sleep_for(chrono::milliseconds(1000 + g() % 2000));
    }
}

int main() {
    int k, l, m;
    cout << "Liczba palaczy (k): ";  cin >> k;
    cout << "Liczba ubijaczy (l): "; cin >> l;
    cout << "Liczba zapalek (m): ";  cin >> m;
    if (k <= 0 || l <= 0 || m <= 0) return 0;

    initscr(); curs_set(0); noecho();

    Kelner kelner(k, l, m);
    vector<thread> ths;
    for (int i = 0; i < k; i++) ths.emplace_back(f_palacz, i, ref(kelner));

    while (true) {
        kelner.rysuj(k);
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    endwin();
}
