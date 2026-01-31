#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

/* Compatibilité GCC 2.95 */
#ifndef IPTR
typedef unsigned long IPTR;
#endif

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) (((ULONG)(a)<<24)|((ULONG)(b)<<16)|((ULONG)(c)<<8)|(ULONG)(d))
#endif

struct Library *MUIMasterBase = NULL;

#define ID_CONNECT 1001
#define ID_SAVE    1002

/* * Nouvelle fonction de sauvegarde 
 * Format: CLÉ=VALEUR
 */
static void SavePrefs(STRPTR host, STRPTR user, STRPTR pass) {
    BPTR fh = Open("S:apollovnc.prefs", MODE_NEWFILE);
    if (fh) {
        /* On écrit explicitement les clés suivies des valeurs */
        FPuts(fh, "SERVER=");   FPuts(fh, host); FPuts(fh, "\n");
        FPuts(fh, "PASSWORD="); FPuts(fh, pass); FPuts(fh, "\n"); /* Attention à l'ordre demandé */
        FPuts(fh, "USERNAME="); FPuts(fh, user); FPuts(fh, "\n");
        Close(fh);
    }
}

/* * Nouvelle fonction de chargement 
 * Elle parse le fichier ligne par ligne pour trouver les clés.
 */
static void LoadPrefs(STRPTR host, STRPTR user, STRPTR pass) {
    BPTR fh = Open("S:apollovnc.prefs", MODE_OLDFILE);
    char line[256]; /* Buffer pour lire une ligne complète */

    /* Valeurs par défaut vides */
    host[0] = 0;
    user[0] = 0;
    pass[0] = 0;

    if (fh) {
        while (FGets(fh, line, 256)) {
            /* Nettoyage du saut de ligne à la fin */
            line[strcspn(line, "\r\n")] = 0;

            /* Comparaison du début de la ligne et extraction de la valeur */
            if (strncmp(line, "SERVER=", 7) == 0) {
                strcpy(host, line + 7); /* Copie ce qui est après "SERVER=" */
            } 
            else if (strncmp(line, "PASSWORD=", 9) == 0) {
                strcpy(pass, line + 9); /* Copie ce qui est après "PASSWORD=" */
            }
            else if (strncmp(line, "USERNAME=", 9) == 0) {
                strcpy(user, line + 9); /* Copie ce qui est après "USERNAME=" */
            }
        }
        Close(fh);
    }
}

int main(void) {
    Object *app, *win, *strHost, *strUser, *strPass;
    Object *btnConnect, *btnSave;
    
    char bufHost[128] = "", bufUser[128] = "", bufPass[128] = "";
    char cmd[512];
    STRPTR h, u, p;
    ULONG signals = 0;
    ULONG res;
    BOOL running = TRUE;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;

    LoadPrefs(bufHost, bufUser, bufPass);

    app = ApplicationObject,
        MUIA_Application_Title,   (IPTR)"ApolloVNC Launcher",
        MUIA_Application_Base,    (IPTR)"APVNC",
        
        SubWindow, win = WindowObject,
            MUIA_Window_Title, (IPTR)"ApolloVNC Connect",
            MUIA_Window_ID,    (IPTR)MAKE_ID('V','N','C','L'),
            MUIA_Window_RootObject, VGroup,
                MUIA_Background, MUII_GroupBack,

                Child, GroupObject,
                    MUIA_Group_Columns, 2,
                    
                    Child, TextObject, MUIA_Text_Contents, (IPTR)"Host IP:", End,
                    Child, strHost = StringObject, MUIA_String_Contents, (IPTR)bufHost, End,
                    
                    Child, TextObject, MUIA_Text_Contents, (IPTR)"User:", End,
                    Child, strUser = StringObject, MUIA_String_Contents, (IPTR)bufUser, End,
                    
                    Child, TextObject, MUIA_Text_Contents, (IPTR)"Password:", End,
                    Child, strPass = StringObject, 
                        MUIA_String_Contents, (IPTR)bufPass,
                        MUIA_String_Secret, TRUE,
                    End,
                End,

                Child, GroupObject,
                    MUIA_Group_Horiz, TRUE,
                    Child, btnConnect = MUI_MakeObject(MUIO_Button, (IPTR)"_Connect"),
                    Child, btnSave    = MUI_MakeObject(MUIO_Button, (IPTR)"_Save"),
                End,
            End,
        End,
    End;

    if (!app) { CloseLibrary(MUIMasterBase); return 20; }

    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2, 
             MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
             
    DoMethod(btnConnect, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, 
             MUIM_Application_ReturnID, ID_CONNECT);

    DoMethod(btnSave, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, 
             MUIM_Application_ReturnID, ID_SAVE);

    set(win, MUIA_Window_Open, TRUE);

    while (running) {
        res = DoMethod(app, MUIM_Application_NewInput, (IPTR)&signals);
        
        if (res == MUIV_Application_ReturnID_Quit) {
            running = FALSE;
        }
        else if (res == ID_CONNECT) {
            get(strHost, MUIA_String_Contents, &h);
            get(strUser, MUIA_String_Contents, &u);
            get(strPass, MUIA_String_Contents, &p);

            /* Lancement de la commande : ApolloVNC IP USER PASS */
            sprintf(cmd, "ApolloVNC %s %s %s", h, u, p);
            Execute((STRPTR)cmd, 0, 0);
        }
        else if (res == ID_SAVE) {
            get(strHost, MUIA_String_Contents, &h);
            get(strUser, MUIA_String_Contents, &u);
            get(strPass, MUIA_String_Contents, &p);

            SavePrefs(h, u, p);
            DisplayBeep(NULL); 
        }

        if (running && signals) {
            signals = Wait(signals | SIGBREAKF_CTRL_C);
            if (signals & SIGBREAKF_CTRL_C) running = FALSE;
        }
    }

    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
