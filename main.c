#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
  #include <winsock2.h>
  typedef SOCKET Socket;
#else
  #error "Mode Online supporté uniquement sous Windows"
#endif

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

typedef enum {
    MENU,
    SELECT_DIFFICULTY,
    GAME_VS_CPU,
    ONLINE_MENU,
    GAME_ONLINE_HOST,
    GAME_ONLINE_CLIENT
} GameState;
typedef enum { EASY, MEDIUM, HARD } Difficulty;

// Structure pour échanger un tir en réseau
typedef struct { float x, y; int pw; } ShotData;

// ——— Globals SDL & Jeu —————————————————————————————————————————
GameState gameState = MENU;
Difficulty currentDifficulty = EASY;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;

SDL_Rect goalkeeper;
float ballX, ballY, ballDirX = 0, ballDirY = -1;
int power = 0;
bool charging = false;
bool ballMoving = false;
bool isPlayerTurn = true;
bool gameFinished = false;

int playerScore = 0, playerShots = 0;
int computerScore = 0, computerShots = 0;
char finalMessage[50] = "";

int goalkeeperDir = 1, goalkeeperSpeed = 2;
Uint32 computerShootTimer= 0;
bool waitingForComputer= false;

// ——— Globals Réseau —————————————————————————————————————————————
Socket listenSock = INVALID_SOCKET;
Socket peerSock = INVALID_SOCKET;

// ——— Prototypes ———————————————————————————————————————————————
void ResetBall(void);
void RenderTextSDL(const char* text,int x,int y,SDL_Color col);
void DrawMenu(void);
void DrawDifficulty(void);
void DrawOnlineMenu(void);
void DrawGoal(void);
void DrawGame(void);
void ComputerShoot(void);

bool InitNetwork(void);
void CloseNetwork(void);
bool HostGame(int port);
bool JoinGame(const char* ip,int port);
bool SendShot(const ShotData* sd);
bool RecvShot(ShotData* sd);

// ——— Implémentation Réseau ——————————————————————————————————————
bool InitNetwork(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2,2), &wsa)==0;
}
void CloseNetwork(void) {
    if(peerSock!=INVALID_SOCKET) closesocket(peerSock);
    if(listenSock!=INVALID_SOCKET) closesocket(listenSock);
    WSACleanup();
}
bool HostGame(int port) {
    struct sockaddr_in sa={0};
    listenSock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    sa.sin_family=AF_INET; sa.sin_addr.s_addr=INADDR_ANY; sa.sin_port=htons(port);
    if(bind(listenSock,(struct sockaddr*)&sa,sizeof(sa))==SOCKET_ERROR) return false;
    listen(listenSock,1);
    peerSock=accept(listenSock,NULL,NULL);
    return peerSock!=INVALID_SOCKET;
}
bool JoinGame(const char* ip,int port) {
    struct sockaddr_in sa={0};
    peerSock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    sa.sin_family=AF_INET; sa.sin_addr.s_addr=inet_addr(ip); sa.sin_port=htons(port);
    return connect(peerSock,(struct sockaddr*)&sa,sizeof(sa))!=SOCKET_ERROR;
}
bool SendShot(const ShotData* sd) {
    int s = send(peerSock,(const char*)sd,sizeof(*sd),0);
    return s==sizeof(*sd);
}
bool RecvShot(ShotData* sd) {
    int r = recv(peerSock,(char*)sd,sizeof(*sd),MSG_WAITALL);
    return r==sizeof(*sd);
}

// ——— Implémentation Jeu Solo/Online —————————————————————————————————
void ResetBall(void) {
    ballX=SCREEN_WIDTH/2; ballY=SCREEN_HEIGHT-100;
    ballDirX=0; ballDirY=-1;
    power=0; ballMoving=false; charging=false;
}

void RenderTextSDL(const char* text,int x,int y,SDL_Color col){
    SDL_Surface* s=TTF_RenderText_Solid(font,text,col);
    SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s);
    SDL_Rect d={x,y,s->w,s->h};
    SDL_FreeSurface(s);
    SDL_RenderCopy(renderer,t,NULL,&d);
    SDL_DestroyTexture(t);
}

void DrawMenu(void){
    SDL_Color w={255,255,255,255};
    RenderTextSDL("Penalty Game",SCREEN_WIDTH/2-80,100,w);
    SDL_Rect b1={SCREEN_WIDTH/2-100,200,200,50}, b2={SCREEN_WIDTH/2-100,300,200,50};
    SDL_SetRenderDrawColor(renderer,50,150,255,255);
    SDL_RenderFillRect(renderer,&b1);
    SDL_RenderFillRect(renderer,&b2);
    RenderTextSDL("Play VS Computer",b1.x+20,b1.y+10,w);
    RenderTextSDL("Play 1v1 Online" ,b2.x+30,b2.y+10,w);
}

void DrawDifficulty(void){
    SDL_Color w={255,255,255,255};
    RenderTextSDL("Select Difficulty",SCREEN_WIDTH/2-80,100,w);
    SDL_Rect btns[3]={{SCREEN_WIDTH/2-100,200,200,50},
                     {SCREEN_WIDTH/2-100,270,200,50},
                     {SCREEN_WIDTH/2-100,340,200,50}};
    const char*t[3]={"Easy","Medium","Hard"};
    for(int i=0;i<3;i++){
        SDL_SetRenderDrawColor(renderer,100+i*50,150,255-i*50,255);
        SDL_RenderFillRect(renderer,&btns[i]);
        RenderTextSDL(t[i],btns[i].x+70,btns[i].y+10,w);
    }
}

void DrawOnlineMenu(void){
    SDL_Color w={255,255,255,255};
    RenderTextSDL("Online Mode",SCREEN_WIDTH/2-80,100,w);
    SDL_Rect h={SCREEN_WIDTH/2-100,200,200,50}, j={SCREEN_WIDTH/2-100,300,200,50};
    SDL_SetRenderDrawColor(renderer,100,200,100,255); SDL_RenderFillRect(renderer,&h);
    SDL_SetRenderDrawColor(renderer,200,100,100,255); SDL_RenderFillRect(renderer,&j);
    RenderTextSDL("Host Game",h.x+60,h.y+10,w);
    RenderTextSDL("Join Game",j.x+60,j.y+10,w);
}

void DrawGoal(void){
    SDL_SetRenderDrawColor(renderer,255,255,255,255);
    SDL_RenderFillRect(renderer,&(SDL_Rect){250,150,300,8});
    SDL_RenderFillRect(renderer,&(SDL_Rect){250,150,8,150});
    SDL_RenderFillRect(renderer,&(SDL_Rect){542,150,8,150});
    SDL_SetRenderDrawColor(renderer,180,180,180,255);
    for(int y=150;y<=300;y+=10) SDL_RenderDrawLine(renderer,250,y,550,y);
    for(int x=250;x<=550;x+=10) SDL_RenderDrawLine(renderer,x,150,x,300);
}

void DrawGame(void){
    SDL_Color w={255,255,255,255};
    char buf[50];
    sprintf(buf,"Player: %d/%d",playerScore,playerShots);
    RenderTextSDL(buf,20,20,w);
    sprintf(buf,"Computer: %d/%d",computerScore,computerShots);
    RenderTextSDL(buf,600,20,w);
    DrawGoal();
    SDL_SetRenderDrawColor(renderer,255,255,255,255);
    SDL_RenderFillRect(renderer,&(SDL_Rect){(int)ballX-10,(int)ballY-10,20,20});
    SDL_SetRenderDrawColor(renderer,255,0,0,255);
    SDL_RenderFillRect(renderer,&goalkeeper);
    float norm=sqrtf(ballDirX*ballDirX+ballDirY*ballDirY);
    SDL_RenderDrawLine(renderer,
        (int)ballX,(int)ballY,
        (int)(ballX+(ballDirX/norm)*50),
        (int)(ballY+(ballDirY/norm)*50)
    );
    RenderTextSDL("Press SPACE to shoot",SCREEN_WIDTH/2-100,540,w);
    SDL_SetRenderDrawColor(renderer,255,0,0,255);
    SDL_RenderFillRect(renderer,&(SDL_Rect){SCREEN_WIDTH/2-100,560,power*4,20});
    if(gameFinished){
        RenderTextSDL(finalMessage,SCREEN_WIDTH/2-60,350,w);
        SDL_Rect b={SCREEN_WIDTH/2-100,400,200,50};
        SDL_SetRenderDrawColor(renderer,0,100,255,255);
        SDL_RenderFillRect(renderer,&b);
        RenderTextSDL("Cliquez ici pour rejouer",b.x+15,b.y+10,w);
    }
}

void ComputerShoot(void){
    ballDirX=((rand()%21)-10)/10.0f;
    ballDirY=-1.0f + (rand()%5)/10.0f;
    power=15+rand()%15;
    ballMoving=true;
}

// ——— main —————————————————————————————————————————————————————
int main(void){
    srand((unsigned)time(NULL));
    InitNetwork();
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    window=SDL_CreateWindow("Penalty Game",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,SCREEN_HEIGHT,SDL_WINDOW_SHOWN
    );
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
    font=TTF_OpenFont("C:/SDL2/projects/penaltygame/bin/Debug/Arial.ttf",24);
    ResetBall();
    goalkeeper=(SDL_Rect){375,150,50,150};

    bool running=true;
    SDL_Event e;
    while(running){
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT){ running=false; break; }
            if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT){
                int x=e.button.x,y=e.button.y;
                if(gameState==MENU){
                    if(x>=300&&x<=500&&y>=200&&y<=250) gameState=SELECT_DIFFICULTY;
                    else if(x>=300&&x<=500&&y>=300&&y<=350) gameState=ONLINE_MENU;
                }
                else if(gameState==SELECT_DIFFICULTY){
                    if(y>=200&&y<=250){ currentDifficulty=EASY; gameState=GAME_VS_CPU; }
                    else if(y>=270&&y<=320){ currentDifficulty=MEDIUM; gameState=GAME_VS_CPU; }
                    else if(y>=340&&y<=390){ currentDifficulty=HARD; gameState=GAME_VS_CPU; }
                    goalkeeperSpeed=currentDifficulty+2;
                }
                else if(gameState==ONLINE_MENU){
                    if(y>=200&&y<=250){
                        if(HostGame(12345)){
                            isPlayerTurn=true; playerShots=computerShots=0;
                            playerScore=computerScore=0; gameFinished=false;
                            ResetBall(); gameState=GAME_ONLINE_HOST;
                        }
                    } else if(y>=300&&y<=350){
                        char ip[64];
                        printf("Enter host IP: ");
                        scanf("%63s",ip);
                        if(JoinGame(ip,12345)){
                            isPlayerTurn=false; playerShots=computerShots=0;
                            playerScore=computerScore=0; gameFinished=false;
                            ResetBall(); gameState=GAME_ONLINE_CLIENT;
                        }
                    }
                }
                else if(gameFinished
                     && x>=300&&x<=500&&y>=400&&y<=450)
                {
                    playerScore=computerScore=0;
                    playerShots=computerShots=0;
                    isPlayerTurn=true; gameFinished=false;
                    ResetBall(); gameState=MENU;
                }
            }

            if((gameState==GAME_VS_CPU ||
                gameState==GAME_ONLINE_HOST ||
                gameState==GAME_ONLINE_CLIENT)
               && isPlayerTurn && !gameFinished
               && e.type==SDL_KEYDOWN)
            {
                if(e.key.keysym.sym==SDLK_LEFT) ballDirX-=0.1f;
                if(e.key.keysym.sym==SDLK_RIGHT) ballDirX+=0.1f;
                if(e.key.keysym.sym==SDLK_UP) ballDirY-=0.1f;
                if(e.key.keysym.sym==SDLK_DOWN) ballDirY+=0.1f;
                if(e.key.keysym.sym==SDLK_SPACE) charging=true;
            }
            if((gameState==GAME_VS_CPU ||
                gameState==GAME_ONLINE_HOST ||
                gameState==GAME_ONLINE_CLIENT)
               && isPlayerTurn && !gameFinished
               && e.type==SDL_KEYUP
               && e.key.keysym.sym==SDLK_SPACE)
            {
                charging=false; ballMoving=true;
                if(gameState==GAME_ONLINE_HOST||gameState==GAME_ONLINE_CLIENT){
                    ShotData sd={ballDirX,ballDirY,power};
                    SendShot(&sd);
                }
            }
        }

        if((gameState==GAME_VS_CPU ||
            gameState==GAME_ONLINE_HOST ||
            gameState==GAME_ONLINE_CLIENT)
           && !gameFinished)
        {
            // gardien
            goalkeeper.x+=goalkeeperDir*goalkeeperSpeed;
            if(goalkeeper.x<=250||goalkeeper.x+goalkeeper.w>=550)
                goalkeeperDir*=-1;
            // charge
            if(charging&&power<50) power++;
            // balle
            if(ballMoving){
                ballX+=ballDirX*power/5.0f; ballY+=ballDirY*power/5.0f;
                if(ballY<=300){
                    bool scored=false;
                    if(ballX>=250&&ballX<=550)
                        if(!(ballX>goalkeeper.x&&ballX<goalkeeper.x+goalkeeper.w))
                            scored=true;
                    if(isPlayerTurn){
                        playerShots++; if(scored) playerScore++;
                        isPlayerTurn=false; waitingForComputer=true;
                        computerShootTimer=SDL_GetTicks();
                    } else {
                        computerShots++; if(scored) computerScore++;
                        isPlayerTurn=true;
                    }
                    ballMoving=false; power=0; ResetBall();
                }
            }

            if ( gameState == GAME_VS_CPU
  && !isPlayerTurn
  && waitingForComputer
  && SDL_GetTicks() - computerShootTimer >= 2000 )
{
    waitingForComputer = false;
    ComputerShoot(); // déclenche enfin le tir de l'ordi en local
}

            // — tir automatique du CPU (mode VS_CPU ou ONLINE) —
if (!isPlayerTurn && waitingForComputer
    && SDL_GetTicks() - computerShootTimer >= 2000)
{
    waitingForComputer = false;

    if (gameState == GAME_VS_CPU) {
        // CPU local
        ComputerShoot();
        isPlayerTurn = true;
    }
    else {
        // CPU en réseau
        ShotData sd;
        if (gameState == GAME_ONLINE_HOST) {
            // je suis l’hôte : je génère et j’envoie
            sd.x = ((rand()%21)-10)/10.0f;
            sd.y = -1.0f + (rand()%5)/10.0f;
            sd.pw= 15 + rand()%15;
            SendShot(&sd);
        } else {
            // je suis le client : je reçois
            RecvShot(&sd);
        }
        ballDirX = sd.x;
        ballDirY = sd.y;
        power = sd.pw;
        ballMoving = true;
        isPlayerTurn = true;
    }
}
            // fin match
            if(playerShots>=5&&computerShots>=5){
                gameFinished=true;
                sprintf(finalMessage,
                  playerScore>computerScore?"YOU WIN":
                  playerScore<computerScore?"COMPUTER WINS":
                  "DRAW"
                );
            }
        }

        // rendu
        SDL_SetRenderDrawColor(renderer,0,128,0,255);
        SDL_RenderClear(renderer);
        switch(gameState){
          case MENU: DrawMenu(); break;
          case SELECT_DIFFICULTY: DrawDifficulty(); break;
          case GAME_VS_CPU: DrawGame(); break;
          case ONLINE_MENU: DrawOnlineMenu(); break;
          case GAME_ONLINE_HOST:
          case GAME_ONLINE_CLIENT: DrawGame(); break;
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    CloseNetwork();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
