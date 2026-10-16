#include <pthread.h>

int sh_game_main(int argc, char** argv);

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
