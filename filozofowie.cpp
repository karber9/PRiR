#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <ncurses.h>
#include <queue>

using namespace std;

int liczba_filozofow;

struct Filozof {
    int id;
    chrono::steady_clock::time_point wait_start;
    Filozof(int id) : id(id), wait_start(chrono::steady_clock::now()) {}

    bool operator<(const Filozof& other) const {
        return wait_start > other.wait_start; // priorytet dla najdłużej czekającego
    }
};

vector<mutex> widelce; // mutex reprezentujący widelce
vector<bool> stan_wid; // stan zasobów

mutex controller_mutex;
condition_variable kelner_cv;
priority_queue<Filozof, vector<Filozof>, less<Filozof>> waiting_queue;
vector<bool> eating;

mutex ncurses_mutex; // do synchronizacji wyświetlania ncurses

// Funkcja do wizualizacji filozofa i postępu
void visualize(int filozof_id, const string& stan, int postep) {
    lock_guard<mutex> lock(ncurses_mutex);
    mvprintw(filozof_id, 0, "Filozof %d:   %s  Postep: %d%%          ", filozof_id + 1, stan.c_str(), postep);
    refresh();
}

// Funkcja generująca losowy czas 
int random_czas(int min, int max) {
    return min + rand() % (max - min + 1);
}

// Kelner
void kelner() {
    while (true) {
        unique_lock<mutex> lock(controller_mutex);
        kelner_cv.wait(lock, [] { return !waiting_queue.empty(); });

        Filozof filozof = waiting_queue.top();
        int lewy = filozof.id;
        int prawy = (filozof.id + 1) % liczba_filozofow;

        if (!stan_wid[lewy] && !stan_wid[prawy]) { //gdy stan obu widelcow=true(wolny)

            waiting_queue.pop();
            stan_wid[lewy] = true;
            stan_wid[prawy] = true;
            eating[filozof.id] = true;
            kelner_cv.notify_all();
        }
    }
}

// Filozof
void filozof(int id) {
    while (true) {
        // Myślenie
        visualize(id, "Mysli     ", 0);

        int czas_myslenia = random_czas(2, 5);  
        for (int i = 0; i < czas_myslenia; ++i) {
            this_thread::sleep_for(chrono::seconds(1));
            visualize(id, "Mysli     ", (i + 1) * 100 / czas_myslenia);
        }

        visualize(id, "Mysli     ", 100);  // Pełen postęp

        {
            lock_guard<mutex> lock(controller_mutex);
            waiting_queue.push(Filozof(id));
        }

        kelner_cv.notify_all();

        {
            unique_lock<mutex> lock(controller_mutex);
            kelner_cv.wait(lock, [&]() { return eating[id]; });
        }

        // Jedzenie
        visualize(id, "Je        ", 0);

        int czas_na_jedzenie = random_czas(2, 5);  
        for (int i = 0; i < czas_na_jedzenie; ++i) {
            this_thread::sleep_for(chrono::seconds(1));
            visualize(id, "Je        ", (i + 1) * 100 / czas_na_jedzenie);
        }

        visualize(id, "Je        ", 100);  // Pełen postęp

        // Odkładanie widelców
        int lewy = id;
        int prawy = (id + 1) % liczba_filozofow;

        {
            lock_guard<mutex> lock(controller_mutex);
            stan_wid[lewy] = false;
            stan_wid[prawy] = false;
            eating[id] = false;
        }

        kelner_cv.notify_all();

        visualize(id, "Skonczyl  ", 0);
        this_thread::sleep_for(chrono::seconds(1));
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "U\u017cycie " << argv[0] << " <liczba_filozofow>";
        return 1;
    }

    liczba_filozofow  = stoi(argv[1]);
    if (liczba_filozofow < 5) {
        cerr << "Liczba filozofow musi byc co najmniej 5!";
        return 1;
    }

    // Inicjalizacja ncurses
    initscr();
    noecho();
    curs_set(FALSE);

    widelce = vector<mutex>(liczba_filozofow);
    stan_wid.resize(liczba_filozofow, false);
    eating.resize(liczba_filozofow, false);

    // Wątek kelnera
    thread controller(kelner);

    // Wątki filozofów
    vector<thread> filozofowie;
    for (int i = 0; i < liczba_filozofow; ++i) {
        filozofowie.emplace_back(filozof, i);
    }

    for (auto& th : filozofowie) {
        th.join();
    }

    controller.join();
    endwin();
    return 0;
}
