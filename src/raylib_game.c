#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#define VENTANA 640
#define TAM_CASILLA 80
#define OFFSET_BORDE 30

typedef enum GameScreen { LOGO = 0, TITLE, GAMEPLAY, ENDING, RULES } GameScreen;
typedef enum { JUGADOR_VS_JUGADOR = 0, JUGADOR_VS_IA } ModoJuego;

GameScreen currentScreen = TITLE;
ModoJuego modoJuego = JUGADOR_VS_JUGADOR;
bool juegoPausado = false;
bool hayPartidaGuardada = false;

int tablero[8][8];
int turnoJugador = 1;
int fichaSeleccionadaX = -1, fichaSeleccionadaY = -1;
bool hayCapturaObligatoria = false;

double tiempoInicioPartida = 0;
double tiempoPausado = 0;
double tiempoTotalPausado = 0;

Sound sonidoMover, sonidoCaptura, sonidoCorona;

void GuardarPartida() {
    FILE *file = fopen("partida.dat", "wb");
    if (file!= NULL) {
        fwrite(&tablero, sizeof(int), 64, file);
        fwrite(&turnoJugador, sizeof(int), 1, file);
        fwrite(&modoJuego, sizeof(int), 1, file);
        fwrite(&currentScreen, sizeof(int), 1, file);
        fwrite(&tiempoInicioPartida, sizeof(double), 1, file);
        fwrite(&tiempoTotalPausado, sizeof(double), 1, file);
        fclose(file);
    }
}

void CargarPartida() {
    FILE *file = fopen("partida.dat", "rb");
    if (file!= NULL) {
        fread(&tablero, sizeof(int), 64, file);
        fread(&turnoJugador, sizeof(int), 1, file);
        fread(&modoJuego, sizeof(int), 1, file);
        fread(&currentScreen, sizeof(int), 1, file);
        fread(&tiempoInicioPartida, sizeof(double), 1, file);
        fread(&tiempoTotalPausado, sizeof(double), 1, file);
        fclose(file);
        hayPartidaGuardada = true;
        juegoPausado = true;
    }
}

void BorrarPartida() {
    remove("partida.dat");
    hayPartidaGuardada = false;
}

void ReiniciarCronometro() {
    tiempoInicioPartida = GetTime();
    tiempoTotalPausado = 0;
    tiempoPausado = 0;
}

void InicializarTablero() {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            tablero[y][x] = 0;
            if ((x + y) % 2 == 1) {
                if (y < 3) tablero[y][x] = 2;
                if (y > 4) tablero[y][x] = 1;
            }
        }
    }
    turnoJugador = 1;
    juegoPausado = false;
    fichaSeleccionadaX = -1;
    fichaSeleccionadaY = -1;
    ReiniciarCronometro();
}

bool EsFichaDelJugador(int ficha) {
    if (turnoJugador == 1) return ficha == 1 || ficha == 3;
    return ficha == 2 || ficha == 4;
}

bool EsDama(int ficha) {
    return ficha == 3 || ficha == 4;
}

bool PuedeCapturar(int x, int y) {
    int ficha = tablero[y][x];
    if (!EsFichaDelJugador(ficha)) return false;
    int dir = (ficha == 1 || ficha == 3)? -1 : 1;
    int dirs[4][2] = {{-1, dir}, {1, dir}, {-1, -dir}, {1, -dir}};
    int maxPasos = EsDama(ficha)? 7 : 1;
    for (int d = 0; d < 4; d++) {
        if (!EsDama(ficha) && d >= 2) continue;
        for (int paso = 1; paso <= maxPasos; paso++) {
            int nx = x + dirs[d][0] * paso;
            int ny = y + dirs[d][1] * paso;
            int nx2 = x + dirs[d][0] * (paso + 1);
            int ny2 = y + dirs[d][1] * (paso + 1);
            if (nx2 < 0 || nx2 >= 8 || ny2 < 0 || ny2 >= 8) break;
            if (tablero[ny][nx] == 0) continue;
            if (EsFichaDelJugador(tablero[ny][nx])) break;
            if (tablero[ny2][nx2] == 0) return true;
            break;
        }
    }
    return false;
}

void VerificarCapturasObligatorias() {
    hayCapturaObligatoria = false;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (PuedeCapturar(x, y)) {
                hayCapturaObligatoria = true;
                return;
            }
        }
    }
}

bool MovimientoValido(int x1, int y1, int x2, int y2, bool *esCaptura) {
    *esCaptura = false;
    if (x2 < 0 || x2 >= 8 || y2 < 0 || y2 >= 8) return false;
    if (tablero[y2][x2]!= 0) return false;
    int ficha = tablero[y1][x1];
    int dx = x2 - x1;
    int dy = y2 - y1;
    if (abs(dx)!= abs(dy)) return false;
    int dir = (ficha == 1 || ficha == 3)? -1 : 1;
    bool esDama = EsDama(ficha);
    if (abs(dx) == 1 &&!hayCapturaObligatoria) {
        if (esDama || (dy == dir)) return true;
    }
    if (abs(dx) == 2) {
        int mx = x1 + dx/2;
        int my = y1 + dy/2;
        if (tablero[my][mx]!= 0 &&!EsFichaDelJugador(tablero[my][mx])) {
            if (esDama || (dy == dir * 2)) {
                *esCaptura = true;
                return true;
            }
        }
    }
    if (esDama && abs(dx) > 2) {
        int stepX = dx / abs(dx);
        int stepY = dy / abs(dy);
        int fichasEnMedio = 0;
        int cx = x1 + stepX, cy = y1 + stepY;
        while (cx!= x2 && cy!= y2) {
            if (tablero[cy][cx]!= 0) {
                if (EsFichaDelJugador(tablero[cy][cx])) return false;
                fichasEnMedio++;
            }
            cx += stepX; cy += stepY;
        }
        if (fichasEnMedio == 0 &&!hayCapturaObligatoria) return true;
        if (fichasEnMedio == 1) {
            *esCaptura = true;
            return true;
        }
    }
    return false;
}

bool MoverFicha(int x1, int y1, int x2, int y2) {
    bool esCaptura = false;
    if (!MovimientoValido(x1, y1, x2, y2, &esCaptura)) return false;
    int ficha = tablero[y1][x1];
    tablero[y2][x2] = ficha;
    tablero[y1][x1] = 0;
    if (esCaptura) {
        int dx = x2 - x1;
        int dy = y2 - y1;
        int stepX = dx / abs(dx);
        int stepY = dy / abs(dy);
        int cx = x1 + stepX, cy = y1 + stepY;
        while (cx!= x2 || cy!= y2) {
            if (tablero[cy][cx]!= 0) {
                tablero[cy][cx] = 0;
                PlaySound(sonidoCaptura);
                break;
            }
            cx += stepX; cy += stepY;
        }
    } else {
        PlaySound(sonidoMover);
    }
    if ((ficha == 1 && y2 == 0) || (ficha == 2 && y2 == 7)) {
        tablero[y2][x2] = ficha + 2;
        PlaySound(sonidoCorona);
        esCaptura = false;
    }
    bool puedeSeguir = esCaptura && PuedeCapturar(x2, y2);
    if (!puedeSeguir) {
        turnoJugador = (turnoJugador == 1)? 2 : 1;
        VerificarCapturasObligatorias();
    }
    GuardarPartida();
    fichaSeleccionadaX = -1;
    fichaSeleccionadaY = -1;
    return true;
}

void MovimientoIA() {
    if (turnoJugador!= 2 || modoJuego!= JUGADOR_VS_IA) return;
    for (int y1 = 0; y1 < 8; y1++) {
        for (int x1 = 0; x1 < 8; x1++) {
            if (EsFichaDelJugador(tablero[y1][x1]) && PuedeCapturar(x1, y1)) {
                for (int y2 = 0; y2 < 8; y2++) {
                    for (int x2 = 0; x2 < 8; x2++) {
                        bool esCaptura = false;
                        if (MovimientoValido(x1, y1, x2, y2, &esCaptura) && esCaptura) {
                            MoverFicha(x1, y1, x2, y2);
                            return;
                        }
                    }
                }
            }
        }
    }
    int intentos = 0;
    while (intentos < 100) {
        int x1 = GetRandomValue(0, 7);
        int y1 = GetRandomValue(0, 7);
        if (!EsFichaDelJugador(tablero[y1][x1])) { intentos++; continue; }
        int x2 = x1 + GetRandomValue(-2, 2);
        int y2 = y1 + GetRandomValue(-2, 2);
        if (MoverFicha(x1, y1, x2, y2)) return;
        intentos++;
    }
}

void ActualizarJuego() {
    if (modoJuego == JUGADOR_VS_IA && turnoJugador == 2 &&!juegoPausado) {
        MovimientoIA();
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&!juegoPausado) {
        if (modoJuego == JUGADOR_VS_IA && turnoJugador == 2) return;
        Vector2 mouse = GetMousePosition();
        int x = (mouse.x - OFFSET_BORDE) / TAM_CASILLA;
        int y = (mouse.y - OFFSET_BORDE) / TAM_CASILLA;
        if (x >= 0 && x < 8 && y >= 0 && y < 8) {
            if (fichaSeleccionadaX == -1) {
                if (EsFichaDelJugador(tablero[y][x])) {
                    if (!hayCapturaObligatoria || PuedeCapturar(x, y)) {
                        fichaSeleccionadaX = x;
                        fichaSeleccionadaY = y;
                    }
                }
            } else {
                MoverFicha(fichaSeleccionadaX, fichaSeleccionadaY, x, y);
            }
        }
    }
}

void DibujarCoordenadas() {
    for (int i = 0; i < 8; i++) {
        char letra[2] = {(char)('A' + i), '\0'};
        DrawText(letra, OFFSET_BORDE + i * TAM_CASILLA + 35, VENTANA + OFFSET_BORDE + 5, 20, BLACK);
    }
    for (int i = 0; i < 8; i++) {
        char numero[2] = {(char)('8' - i), '\0'};
        DrawText(numero, 10, OFFSET_BORDE + i * TAM_CASILLA + 30, 20, BLACK);
    }
}

void DibujarTablero() {
    DibujarCoordenadas();
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            Color color = ((x + y) % 2 == 0)? BEIGE : BROWN;
            DrawRectangle(OFFSET_BORDE + x * TAM_CASILLA, OFFSET_BORDE + y * TAM_CASILLA, TAM_CASILLA, TAM_CASILLA, color);
            if (x == fichaSeleccionadaX && y == fichaSeleccionadaY) {
                DrawRectangleLinesEx((Rectangle){OFFSET_BORDE + x * TAM_CASILLA, OFFSET_BORDE + y * TAM_CASILLA, TAM_CASILLA, TAM_CASILLA}, 3, YELLOW);
            }
            int ficha = tablero[y][x];
            if (ficha == 1) DrawCircle(OFFSET_BORDE + x * TAM_CASILLA + 40, OFFSET_BORDE + y * TAM_CASILLA + 40, 30, WHITE);
            if (ficha == 2) DrawCircle(OFFSET_BORDE + x * TAM_CASILLA + 40, OFFSET_BORDE + y * TAM_CASILLA + 40, 30, BLACK);
            if (ficha == 3) {
                DrawCircle(OFFSET_BORDE + x * TAM_CASILLA + 40, OFFSET_BORDE + y * TAM_CASILLA + 40, 30, WHITE);
                DrawCircle(OFFSET_BORDE + x * TAM_CASILLA + 40, OFFSET_BORDE + y * TAM_CASILLA + 40, 15, LIGHTGRAY);
                DrawText("D", OFFSET_BORDE + x * TAM_CASILLA + 35, OFFSET_BORDE + y * TAM_CASILLA + 30, 20, BLACK);
            }
            if (ficha == 4) {
                DrawCircle(OFFSET_BORDE + x * TAM_CASILLA + 40, OFFSET_BORDE + y * TAM_CASILLA + 40, 30, BLACK);
                DrawCircle(OFFSET_BORDE + x * TAM_CASILLA + 40, OFFSET_BORDE + y * TAM_CASILLA + 40, 15, DARKGRAY);
                DrawText("D", OFFSET_BORDE + x * TAM_CASILLA + 35, OFFSET_BORDE + y * TAM_CASILLA + 30, 20, WHITE);
            }
        }
    }
}

void DibujarCronometro() {
    double tiempoActual = juegoPausado? tiempoPausado : GetTime() - tiempoTotalPausado - tiempoInicioPartida;
    int minutos = (int)tiempoActual / 60;
    int segundos = (int)tiempoActual % 60;
    DrawText(TextFormat("Tiempo: %02d:%02d", minutos, segundos), VENTANA + OFFSET_BORDE + 10, 160, 20, BLACK);
}

int main(void) {
    InitWindow(VENTANA + OFFSET_BORDE * 2 + 60, VENTANA + OFFSET_BORDE * 2 + 60, "Damas G1");
    InitAudioDevice();
    sonidoMover = LoadSound("assets/move.wav");
    sonidoCaptura = LoadSound("assets/capture.wav");
    sonidoCorona = LoadSound("assets/king.wav");
    SetTargetFPS(60);
    CargarPartida();
    if (!hayPartidaGuardada) {
        InicializarTablero();
    } else {
        tiempoPausado = GetTime() - tiempoTotalPausado - tiempoInicioPartida;
    }
    VerificarCapturasObligatorias();
    while (!WindowShouldClose()) {
        switch(currentScreen) {
            case TITLE: {
                Rectangle btnJvJ = { VENTANA/2 - 100, 200, 200, 50 };
                Rectangle btnJvIA = { VENTANA/2 - 100, 270, 200, 50 };
                Rectangle btnReglas = { VENTANA/2 - 100, 340, 200, 50 };
                Rectangle btnContinuar = { VENTANA/2 - 100, 410, 200, 50 };
                if (CheckCollisionPointRec(GetMousePosition(), btnJvJ) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    modoJuego = JUGADOR_VS_JUGADOR;
                    BorrarPartida();
                    InicializarTablero();
                    currentScreen = GAMEPLAY;
                    VerificarCapturasObligatorias();
                }
                if (CheckCollisionPointRec(GetMousePosition(), btnJvIA) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    modoJuego = JUGADOR_VS_IA;
                    BorrarPartida();
                    InicializarTablero();
                    currentScreen = GAMEPLAY;
                    VerificarCapturasObligatorias();
                }
                if (CheckCollisionPointRec(GetMousePosition(), btnReglas) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    currentScreen = RULES;
                }
                if (hayPartidaGuardada && CheckCollisionPointRec(GetMousePosition(), btnContinuar) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    currentScreen = GAMEPLAY;
                    juegoPausado = true;
                    tiempoPausado = GetTime() - tiempoTotalPausado - tiempoInicioPartida;
                }
                BeginDrawing();
                ClearBackground(RAYWHITE);
                DrawText("DAMAS G1", VENTANA/2 - 100, 100, 40, BLACK);
                DrawRectangleRec(btnJvJ, LIGHTGRAY);
                DrawText("1 VS 1", btnJvJ.x + 70, btnJvJ.y + 15, 20, BLACK);
                DrawRectangleRec(btnJvIA, LIGHTGRAY);
                DrawText("VS IA", btnJvIA.x + 75, btnJvIA.y + 15, 20, BLACK);
                DrawRectangleRec(btnReglas, LIGHTGRAY);
                DrawText("REGLAS", btnReglas.x + 65, btnReglas.y + 15, 20, BLACK);
                if (hayPartidaGuardada) {
                    DrawRectangleRec(btnContinuar, GREEN);
                    DrawText("CONTINUAR", btnContinuar.x + 45, btnContinuar.y + 15, 20, WHITE);
                }
                EndDrawing();
            } break;
            case RULES: {
                if (IsKeyPressed(KEY_ESCAPE)) currentScreen = TITLE;
                BeginDrawing();
                ClearBackground(BEIGE);
                DrawText("REGLAS DE DAMAS", 20, 20, 30, BLACK);
                DrawText("- Blancas mueven primero", 20, 70, 20, DARKGRAY);
                DrawText("- Mueves en diagonal 1 casilla", 20, 100, 20, DARKGRAY);
                DrawText("- Comes saltando fichas enemigas", 20, 130, 20, DARKGRAY);
                DrawText("- Captura es OBLIGATORIA", 20, 160, 20, MAROON);
                DrawText("- Si llegas al final = DAMA", 20, 190, 20, DARKGRAY);
                DrawText("- DAMA se mueve varias casillas", 20, 220, 20, DARKGRAY);
                DrawText("- Tecla P = Pausa", 20, 280, 20, MAROON);
                DrawText("- ESC para volver al menu", 20, 500, 20, RED);
                EndDrawing();
            } break;
            case GAMEPLAY: {
                if (IsKeyPressed(KEY_P)) {
                    if (!juegoPausado) {
                        tiempoPausado = GetTime() - tiempoTotalPausado - tiempoInicioPartida;
                    } else {
                        tiempoTotalPausado += GetTime() - tiempoPausado - tiempoInicioPartida;
                    }
                    juegoPausado =!juegoPausado;
                    if (juegoPausado) GuardarPartida();
                }
                if (!juegoPausado) ActualizarJuego();
                BeginDrawing();
                ClearBackground(RAYWHITE);
                DibujarTablero();
                Rectangle btnPausa = { VENTANA + OFFSET_BORDE + 10, 10, 40, 40 };
                DrawRectangleRec(btnPausa, GRAY);
                DrawText("||", btnPausa.x + 15, btnPausa.y + 10, 20, WHITE);
                if (CheckCollisionPointRec(GetMousePosition(), btnPausa) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (!juegoPausado) {
                        tiempoPausado = GetTime() - tiempoTotalPausado - tiempoInicioPartida;
                    } else {
                        tiempoTotalPausado += GetTime() - tiempoPausado - tiempoInicioPartida;
                    }
                    juegoPausado =!juegoPausado;
                    if (juegoPausado) GuardarPartida();
                }
                DrawText(TextFormat("Turno: %s", turnoJugador == 1? "BLANCAS" : "NEGRAS"), VENTANA + OFFSET_BORDE + 10, 70, 20, BLACK);
                DrawText(TextFormat("Modo: %s", modoJuego == 0? "1v1" : "VS IA"), VENTANA + OFFSET_BORDE + 10, 100, 20, BLACK);
                if (hayCapturaObligatoria) DrawText("¡CAPTURA OBLIGATORIA!", VENTANA + OFFSET_BORDE + 10, 130, 18, RED);
                DibujarCronometro();
                if (juegoPausado) {
                    DrawRectangle(0, 0, VENTANA + OFFSET_BORDE * 2 + 60, VENTANA + OFFSET_BORDE * 2 + 60, ColorAlpha(BLACK, 0.7f));
                    DrawText("PAUSA", VENTANA/2 - 60, VENTANA/2 - 100, 40, WHITE);
                    Rectangle btnReanudar = { VENTANA/2 - 100, VENTANA/2 - 20, 200, 50 };
                    Rectangle btnMenu = { VENTANA/2 - 100, VENTANA/2 + 50, 200, 50 };
                    DrawRectangleRec(btnReanudar, GREEN);
                    DrawText("Reanudar", btnReanudar.x + 55, btnReanudar.y + 15, 20, WHITE);
                    DrawRectangleRec(btnMenu, RED);
                    DrawText("Menu Principal", btnMenu.x + 30, btnMenu.y + 15, 20, WHITE);
                    if (CheckCollisionPointRec(GetMousePosition(), btnReanudar) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        tiempoTotalPausado += GetTime() - tiempoPausado - tiempoInicioPartida;
                        juegoPausado = false;
                    }
                    if (CheckCollisionPointRec(GetMousePosition(), btnMenu) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        GuardarPartida();
                        currentScreen = TITLE;
                        juegoPausado = false;
                    }
                }
                EndDrawing();
            } break;
            default: break;
        }
    }
    GuardarPartida();
    UnloadSound(sonidoMover);
    UnloadSound(sonidoCaptura);
    UnloadSound(sonidoCorona);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
