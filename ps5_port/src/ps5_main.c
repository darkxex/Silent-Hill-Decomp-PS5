#include <pthread.h>

int sh_game_main(int argc, char** argv);
int ps5_elevate(void);
void ps5_install_assets(void);

struct main_args
{
    int argc;
    char** argv;
    int result;
};

static void* thread_entry(void* p)
{
    struct main_args* a = (struct main_args*)p;

    a->result = sh_game_main(a->argc, a->argv);
    return NULL;
}

int main(int argc, char** argv)
{
    struct main_args a;
    pthread_attr_t attr;
    pthread_t thread;

    /* The shipped assets are read from /app0 while the sandbox view is still intact: once the
     * HEN lifts the process out of it, /app0 and /download0 are no longer visible. */
    ps5_install_assets();
    /* The HEN grant is asked for before any thread of ours exists (see ps5_unlock.c). */
    ps5_elevate();

    a.argc = argc;
    a.argv = argv;
    a.result = 0;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 128u * 1024u * 1024u);
    if (pthread_create(&thread, &attr, thread_entry, &a) != 0)
    {
        return sh_game_main(argc, argv);
    }
    pthread_join(thread, NULL);
    return a.result;
}
