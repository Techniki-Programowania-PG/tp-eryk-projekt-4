#include <iostream>
#include <vector>
#include <list>
#include <set>
#include <string>
#include <algorithm>
#include <cmath>

#include <SDL.h>
#include <SDL_ttf.h>

const int SZEROKOSC_OKNA = 800;
const int WYSOKOSC_OKNA = 600;
const int LICZBA_PIETER = 6;
const int WYSOKOSC_PIETRA = WYSOKOSC_OKNA / LICZBA_PIETER;
const float PREDKOSC_WINDY = 3.0f;
const float PREDKOSC_PASAZERA = 150.0f;
const int MAKSYMALNA_WAGA = 600;
const int SREDNIA_WAGA_PASAZERA = 70;
const int CZAS_POWROTU_NA_PARTER = 5000;

enum class Kierunek { STOI, GORA, DOL };
enum class StanWinda { STOI_ZAMKNIETA, RUCH, STOI_OTWARTA };

struct Pasazer {
    enum Stan { CZEKA, CZEKA_NA_WYBOR_CELU, WSIADA, W_WINDZIE, WYSIADA, DO_USUNIECIA };

    int id;
    float x, y;
    int pietroStartowe;
    int pietroDocelowe;
    Stan stan;
};

void rysujTekst(SDL_Renderer* renderer, TTF_Font* font, const std::string& tekst, int x, int y, SDL_Color kolor);

void rysujPasazera(SDL_Renderer* renderer, const Pasazer& p) {
    int ix = static_cast<int>(p.x);
    int iy = static_cast<int>(p.y);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect glowa = { ix - 5, iy - 20, 10, 10 };
    SDL_RenderFillRect(renderer, &glowa);
    SDL_RenderDrawLine(renderer, ix, iy - 10, ix, iy + 5);
    SDL_RenderDrawLine(renderer, ix, iy, ix - 8, iy - 5);
    SDL_RenderDrawLine(renderer, ix, iy, ix + 8, iy - 5);
    SDL_RenderDrawLine(renderer, ix, iy + 5, ix - 8, iy + 15);
    SDL_RenderDrawLine(renderer, ix, iy + 5, ix + 8, iy + 15);
}


class Winda {
private:
    float aktualnePietroF;
    Kierunek kierunek;
    StanWinda stan;

    std::set<int> kolejkaWewnetrzna;
    std::set<int> kolejkaZewnetrznaGora;
    std::set<int> kolejkaZewnetrznaDol;

    Uint32 czasOstatniegoZatrzymania;
    bool odliczanieDoPowrotu;

    std::list<Pasazer> pasazerowie;
    int nextPasazerId = 0;

public:
    Winda() : aktualnePietroF(0), kierunek(Kierunek::STOI), stan(StanWinda::STOI_ZAMKNIETA), czasOstatniegoZatrzymania(0), odliczanieDoPowrotu(false) {}

    void aktualizuj(float deltaTime) {
        if (stan == StanWinda::RUCH) {
            int docelowePietro = *kolejkaWewnetrzna.begin();
            if (!kolejkaWewnetrzna.empty()) docelowePietro = *kolejkaWewnetrzna.begin();
            else if (!kolejkaZewnetrznaGora.empty()) docelowePietro = *kolejkaZewnetrznaGora.begin();
            else if (!kolejkaZewnetrznaDol.empty()) docelowePietro = *kolejkaZewnetrznaDol.rbegin();

            if (kierunek == Kierunek::GORA) aktualnePietroF += PREDKOSC_WINDY * deltaTime;
            else if (kierunek == Kierunek::DOL) aktualnePietroF -= PREDKOSC_WINDY * deltaTime;

            if ((kierunek == Kierunek::GORA && aktualnePietroF >= docelowePietro) || (kierunek == Kierunek::DOL && aktualnePietroF <= docelowePietro)) {
                zatrzymajNaPietrze(docelowePietro);
            }
        }

        for (auto& p : pasazerowie) {
            aktualizujPasazera(p, deltaTime);
        }
        pasazerowie.remove_if([](const Pasazer& p) { return p.stan == Pasazer::Stan::DO_USUNIECIA; });

        if (stan == StanWinda::STOI_ZAMKNIETA) {
            if (kolejkiPuste() && getLiczbaPasazerow() == 0) {
                if (!odliczanieDoPowrotu) {
                    odliczanieDoPowrotu = true;
                    czasOstatniegoZatrzymania = SDL_GetTicks();
                }
                else if (SDL_GetTicks() - czasOstatniegoZatrzymania > CZAS_POWROTU_NA_PARTER) {
                    wezwij(0, Kierunek::STOI);
                    odliczanieDoPowrotu = false;
                }
            }
            else {
                wybierzNastepnyCel();
            }
        }
    }

    void aktualizujPasazera(Pasazer& p, float deltaTime) {
        int pietroY = WYSOKOSC_OKNA - (p.pietroStartowe * WYSOKOSC_PIETRA) - 40;
        int docelowyXWsiadanie = SZEROKOSC_OKNA / 2;
        int docelowyXWysiadanie = SZEROKOSC_OKNA / 2 + 150;

        switch (p.stan) {
        case Pasazer::Stan::WSIADA:
            p.x += PREDKOSC_PASAZERA * deltaTime;
            if (p.x >= docelowyXWsiadanie) {
                p.x = docelowyXWsiadanie;
                p.stan = Pasazer::Stan::W_WINDZIE;
            }
            break;
        case Pasazer::Stan::W_WINDZIE:
            p.y = WYSOKOSC_OKNA - (aktualnePietroF * WYSOKOSC_PIETRA) - 40;
            break;
        case Pasazer::Stan::WYSIADA:
            p.x += PREDKOSC_PASAZERA * deltaTime;
            if (p.x >= docelowyXWysiadanie) {
                p.stan = Pasazer::Stan::DO_USUNIECIA;
            }
            break;
        default: break;
        }
    }

    void wezwij(int pietro, Kierunek kierunekWezwania) {
        if (getAktualnaWaga() + SREDNIA_WAGA_PASAZERA > MAKSYMALNA_WAGA) {
            std::cout << "[INFO] Winda pelna, nie mozna wezwac." << std::endl; return;
        }

        Pasazer p;
        p.id = nextPasazerId++;
        p.pietroStartowe = pietro;
        p.stan = Pasazer::Stan::CZEKA;
        p.x = SZEROKOSC_OKNA / 2 + 100;
        p.y = WYSOKOSC_OKNA - (pietro * WYSOKOSC_PIETRA) - 40;
        pasazerowie.push_back(p);

        if (kierunekWezwania == Kierunek::GORA) kolejkaZewnetrznaGora.insert(pietro);
        else if (kierunekWezwania == Kierunek::DOL) kolejkaZewnetrznaDol.insert(pietro);
        else kolejkaWewnetrzna.insert(pietro);

        odliczanieDoPowrotu = false;
        if (stan == StanWinda::STOI_ZAMKNIETA) wybierzNastepnyCel();
    }

    void ustawCelDlaCzekajacegoPasazera(int pietroCel) {
        int pietro windy = round(aktualnePietroF);
        for (auto& p : pasazerowie) {
            if (p.pietroStartowe == pietro && p.stan == Pasazer::Stan::CZEKA_NA_WYBOR_CELU) {
                p.pietroDocelowe = pietroCel;
                p.stan = Pasazer::Stan::WSIADA;
                kolejkaWewnetrzna.insert(pietroCel);
                stan = StanWinda::STOI_ZAMKNIETA;
                std::cout << "[PASAZER] Wsiada, cel: " << pietroCel << std::endl;
                return;
            }
        }
    }

    void rysuj(SDL_Renderer* renderer) {
        SDL_Rect szybRect = { SZEROKOSC_OKNA / 2 - 50, 0, 100, WYSOKOSC_OKNA };
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &szybRect);

        SDL_Rect windaRect;
        windaRect.w = 80;
        windaRect.h = WYSOKOSC_PIETRA;
        windaRect.x = SZEROKOSC_OKNA / 2 - 40;
        windaRect.y = static_cast<int>(WYSOKOSC_OKNA - (aktualnePietroF + 1) * WYSOKOSC_PIETRA);
        SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
        SDL_RenderFillRect(renderer, &windaRect);

        for (const auto& p : pasazerowie) {
            rysujPasazera(renderer, p);
        }
    }

private:
    void zatrzymajNaPietrze(int pietro) {
        aktualnePietroF = static_cast<float>(pietro);
        kierunek = Kierunek::STOI;
        stan = StanWinda::STOI_ZAMKNIETA;

        for (auto& p : pasazerowie) {
            if (p.stan == Pasazer::Stan::W_WINDZIE && p.pietroDocelowe == pietro) {
                p.stan = Pasazer::Stan::WYSIADA;
                std::cout << "[PASAZER] Wysiada na pietrze " << pietro << std::endl;
            }
        }
        kolejkaWewnetrzna.erase(pietro);

        bool czyKtosCzeka = false;
        for (auto& p : pasazerowie) {
            if (p.stan == Pasazer::Stan::CZEKA && p.pietroStartowe == pietro) {
                p.stan = Pasazer::Stan::CZEKA_NA_WYBOR_CELU;
                czyKtosCzeka = true;
            }
        }

        if (czyKtosCzeka) {
            stan = StanWinda::STOI_OTWARTA;
        }

        kolejkaZewnetrznaGora.erase(pietro);
        kolejkaZewnetrznaDol.erase(pietro);
    }

    void wybierzNastepnyCel() {
        if (kolejkiPuste()) return;

        int pietro = round(aktualnePietroF);

        int najblizszyCel = -1;
        int minOdleglosc = LICZBA_PIETER;

        auto znajdz = [&](const std::set<int>& kolejka) {
            for (int cel : kolejka) {
                if (abs(cel - pietro) < minOdleglosc) {
                    minOdleglosc = abs(cel - pietro);
                    najblizszyCel = cel;
                }
            }
            };
        znajdz(kolejkaWewnetrzna);
        znajdz(kolejkaZewnetrznaGora);
        znajdz(kolejkaZewnetrznaDol);

        if (najblizszyCel != -1) {
            if (najblizszyCel > pietro) kierunek = Kierunek::GORA;
            else if (najblizszyCel < pietro) kierunek = Kierunek::DOL;
            else { zatrzymajNaPietrze(pietro); return; }
            stan = StanWinda::RUCH;
        }
    }

public:
    StanWinda getStan() const { return stan; }
    bool kolejkiPuste() const { return kolejkaWewnetrzna.empty() && kolejkaZewnetrznaGora.empty() && kolejkaZewnetrznaDol.empty(); }
    int getLiczbaPasazerow() const {
        int count = 0;
        for (const auto& p : pasazerowie) if (p.stan == Pasazer::Stan::W_WINDZIE) count++;
        return count;
    }
    int getAktualnaWaga() const { return getLiczbaPasazerow() * SREDNIA_WAGA_PASAZERA; }
};

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* okno = SDL_CreateWindow("Symulator Windy v3 - Z Pasażerami", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SZEROKOSC_OKNA, WYSOKOSC_OKNA, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(okno, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    TTF_Font* font = TTF_OpenFont("Arial.ttf", 18);
    if (!font) { std::cerr << "Brak czcionki Arial.ttf" << std::endl; return 1; }

    Winda winda;
    std::vector<SDL_Rect> przyciskiGora(LICZBA_PIETER), przyciskiDol(LICZBA_PIETER), przyciskiWewnetrzne(LICZBA_PIETER);
    for (int i = 0; i < LICZBA_PIETER; ++i) {
        if (i < LICZBA_PIETER - 1) przyciskiGora[i] = { SZEROKOSC_OKNA / 2 + 60, WYSOKOSC_OKNA - (i * WYSOKOSC_PIETRA) - 70, 25, 25 };
        if (i > 0) przyciskiDol[i] = { SZEROKOSC_OKNA / 2 + 60, WYSOKOSC_OKNA - (i * WYSOKOSC_PIETRA) - 40, 25, 25 };
        przyciskiWewnetrzne[i] = { SZEROKOSC_OKNA - 80, WYSOKOSC_OKNA - (i * WYSOKOSC_PIETRA) - 75, 50, 50 };
    }

    bool dziala = true;
    Uint32 ostatniCzas = SDL_GetTicks();
    while (dziala) {
        Uint32 aktualnyCzas = SDL_GetTicks();
        float deltaTime = (aktualnyCzas - ostatniCzas) / 1000.0f;
        ostatniCzas = aktualnyCzas;

        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) dziala = false;
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                SDL_Point p = { mx, my };
                for (int i = 0; i < LICZBA_PIETER; ++i) {
                    if (i < LICZBA_PIETER - 1 && SDL_PointInRect(&p, &przyciskiGora[i])) winda.wezwij(i, Kierunek::GORA);
                    if (i > 0 && SDL_PointInRect(&p, &przyciskiDol[i])) winda.wezwij(i, Kierunek::DOL);
                }
                if (winda.getStan() == StanWinda::STOI_OTWARTA) {
                    for (int i = 0; i < LICZBA_PIETER; ++i) {
                        if (SDL_PointInRect(&p, &przyciskiWewnetrzne[i])) {
                            winda.ustawCelDlaCzekajacegoPasazera(i);
                            break;
                        }
                    }
                }
            }
        }

        winda.aktualizuj(deltaTime);

        SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255);
        SDL_RenderClear(renderer);

        for (int i = 0; i < LICZBA_PIETER; ++i) {
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_RenderDrawLine(renderer, 0, WYSOKOSC_OKNA - i * WYSOKOSC_PIETRA, SZEROKOSC_OKNA, WYSOKOSC_OKNA - i * WYSOKOSC_PIETRA);
            rysujTekst(renderer, font, std::to_string(i), SZEROKOSC_OKNA / 2 - 80, WYSOKOSC_OKNA - (i * WYSOKOSC_PIETRA) - 30, { 255, 255, 255 });
            if (i < LICZBA_PIETER - 1) { SDL_SetRenderDrawColor(renderer, 0, 180, 0, 255); SDL_RenderFillRect(renderer, &przyciskiGora[i]); rysujTekst(renderer, font, "^", przyciskiGora[i].x + 7, przyciskiGora[i].y, { 255,255,255 }); }
            if (i > 0) { SDL_SetRenderDrawColor(renderer, 180, 0, 0, 255); SDL_RenderFillRect(renderer, &przyciskiDol[i]); rysujTekst(renderer, font, "v", przyciskiDol[i].x + 7, przyciskiDol[i].y, { 255,255,255 }); }
        }

        winda.rysuj(renderer);

        if (winda.getStan() == StanWinda::STOI_OTWARTA) {
            rysujTekst(renderer, font, "Wybierz pietro:", SZEROKOSC_OKNA - 110, 10, { 255, 255, 0 });
            for (int i = 0; i < LICZBA_PIETER; ++i) {
                SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255); SDL_RenderFillRect(renderer, &przyciskiWewnetrzne[i]);
                rysujTekst(renderer, font, std::to_string(i), przyciskiWewnetrzne[i].x + 20, przyciskiWewnetrzne[i].y + 15, { 255, 255, 255 });
            }
        }

        rysujTekst(renderer, font, "Waga: " + std::to_string(winda.getAktualnaWaga()) + " kg", 10, 10, { 255, 255, 255 });
        rysujTekst(renderer, font, "Pasazerowie: " + std::to_string(winda.getLiczbaPasazerow()), 10, 40, { 255, 255, 255 });

        SDL_RenderPresent(renderer);
    }

    TTF_CloseFont(font); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(okno);
    TTF_Quit(); SDL_Quit();
    return 0;
}

void rysujTekst(SDL_Renderer* renderer, TTF_Font* font, const std::string& tekst, int x, int y, SDL_Color kolor) {
    if (tekst.empty() || !font) return;
    SDL_Surface* surface = TTF_RenderText_Blended(font, tekst.c_str(), kolor);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dstRect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &dstRect);
    SDL_FreeSurface(surface); SDL_DestroyTexture(texture);
}