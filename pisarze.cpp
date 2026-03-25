#include <ncurses.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <random>
#include <iostream>
#include <string>

using namespace std;

struct Buffer {
    int val = 0;
    int reads = 0;
    bool busy = false;
    int version = 0;
};

struct StanCzytelnika {
    string status = "czeka";
    int czyta_val = -1;
};

class Kelner {
    mutex m;
    condition_variable cv_p, cv_c;
    queue<int> fifo;
    int ilosc_czytelnikow = 0;
    bool ktos_pisze = false;  
    vector<Buffer> bufory;
    int limit_odczytow= 3;

public:
    vector<StanCzytelnika> stan_cz;

    Kelner(int n_p, int n_c) {
        bufory.resize(n_p);
        stan_cz.resize(n_c);
    }

    void pisarz_chce(int id) {
        unique_lock<mutex> lock(m);
        fifo.push(id);
        cv_p.wait(lock, [&]{
            return fifo.front() == id && ilosc_czytelnikow == 0 && !ktos_pisze;          // czekaj az nikt nie pisze
        });

        fifo.pop();
        bufory[id].busy= true;
        bufory[id].reads = 0;
        ktos_pisze = true;         
    }

    void pisarz_konczy(int id, int v) {
        lock_guard<mutex> lock(m);
        bufory[id].val= v;
        bufory[id].busy= false;
        bufory[id].version++;
        ktos_pisze= false;      
        cv_c.notify_all();
        cv_p.notify_all();
    }

    void czekaj_na_odczyty(int id) {
        unique_lock<mutex> lock(m);
        cv_p.wait(lock, [&]{ return bufory[id].reads >= limit_odczytow; });
    }

    struct CzytajWynik { int val; int ver; int buf_id; };

    CzytajWynik czytelnik_start(int cz_id, vector<int>& znane_wersje) {
        unique_lock<mutex> lock(m);
        int b_id = -1;
        cv_c.wait(lock, [&]{
            for (int i = 0; i < (int)bufory.size(); i++) {
                if (!bufory[i].busy && bufory[i].version > znane_wersje[i]) {
                    b_id = i;
                    return true;
                }
            }
            return false;
        });
        ilosc_czytelnikow++;
        stan_cz[cz_id].status= "czyta P" + to_string(b_id);
        stan_cz[cz_id].czyta_val = bufory[b_id].val;
        
        return {bufory[b_id].val, bufory[b_id].version, b_id};
    }

    void czytelnik_stop(int cz_id, int b_id) {
        lock_guard<mutex> lock(m);
        if (b_id != -1) bufory[b_id].reads++;
        ilosc_czytelnikow--;
        stan_cz[cz_id].status= "odpoczywa";
        stan_cz[cz_id].czyta_val = -1;
        cv_p.notify_all();
    }

    void rysuj(int n_p, int n_c) {
        lock_guard<mutex> lock(m);
        erase();
        mvprintw(0, 0, "PISARZ   WARTOSC   ODCZYTY   STATUS");
        

        for (int i = 0; i < n_p; i++) {
            string st;
            if (bufory[i].busy) st = "ZAPISUJE";
                
            else if (bufory[i].reads < limit_odczytow && bufory[i].version > 0)
                st = "OCZEKIWANY (" + to_string(bufory[i].reads) + "/3)";
                
            else st = "KOLEJKA";
            mvprintw(2+i, 0, "P%-5d | %7d | %7d | %s", i, bufory[i].val, bufory[i].reads, st.c_str());
        }
        int row = 3 + n_p;
        mvprintw(row++, 0, "KOLEJKA FIFO: ");
        queue<int> tmp = fifo;
        while (!tmp.empty()) { 
            printw("%d ", tmp.front()); tmp.pop(); 
        }
                
        row++;
        mvprintw(row++, 0, "CZYTELNIK  STATUS       CZYTA");
        
        for (int i = 0; i < n_c; i++) {
            auto& s = stan_cz[i];
            mvprintw(row++, 0, "C%-8d | %-11s | %s", i, s.status.c_str(), 
                s.czyta_val >= 0 ? to_string(s.czyta_val).c_str() : "---");
        }
        
        refresh();
    }
};

void f_pisarz(int id, Kelner& k) {

    random_device rd; mt19937 g(rd());
    
    while (true) {
        int nowa_liczba = g() % 100;
        this_thread::sleep_for(chrono::milliseconds(g() % 3000));
        k.pisarz_chce(id);
        this_thread::sleep_for(chrono::milliseconds(1000));
        k.pisarz_konczy(id, nowa_liczba);
        k.czekaj_na_odczyty(id);
        this_thread::sleep_for(chrono::seconds(1));
    }
}

void f_czytelnik(int id, Kelner& k, int n_p) {
    random_device rd; mt19937 g(rd());
    vector<int> znane_wersje(n_p, 0);
    while (true) {
        auto res = k.czytelnik_start(id, znane_wersje);
        this_thread::sleep_for(chrono::milliseconds(1000 + g() % 1500));
        znane_wersje[res.buf_id] = res.ver;
        k.czytelnik_stop(id, res.buf_id);
        this_thread::sleep_for(chrono::milliseconds(g() % 3000));
    }
}

int main() {
    int n_p, n_c;
    cout << "Liczba pisarzy: ";cin >> n_p;
    cout << "Liczba czytelnikow: "; cin >> n_c;
    if (n_p <= 0 || n_c <= 0) return 0;

    initscr(); curs_set(0); noecho();

    Kelner k(n_p, n_c);
    vector<thread> ths;
    for (int i = 0; i < n_p; i++) ths.emplace_back(f_pisarz, i, ref(k));
    for (int i = 0; i < n_c; i++) ths.emplace_back(f_czytelnik, i, ref(k), n_p);

    while (true) {
        k.rysuj(n_p, n_c);
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    endwin();
}
