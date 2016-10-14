#include "tci.h"

#include <stdlib.h>
#include <math.h>

#if TCI_USE_OPENMP_THREADS
#include <omp.h>
#elif TCI_USE_PTHREADS_THREADS
#include <pthread.h>
#endif

#if TCI_USE_OPENMP_THREADS

int tci_parallelize_int(tci_thread_func_t func, void* payload, int nthread, int arity)
{
    tci_context_t* context;
    int ret = tci_context_init(&context, nthread, arity);
    if (ret != 0) return ret;

    #pragma omp parallel num_threads(nthread)
    {
        tci_comm_t comm;
        tci_comm_init(&comm, context, omp_get_thread_num(), 1, 0);
        func(&comm, payload);
        #pragma omp barrier
        tci_comm_destroy(&comm);
    }

    return 0;
}

#elif TCI_USE_PTHREADS_THREADS

typedef struct
{
    tci_thread_func_t func
    void* payload;
    tci_context_t* context;
    int tid;
} tci_thread_data_t;

void* tci_run_thread(void* raw_data)
{
    tci_thread_data_t* data = (tci_thread_data_t*)raw_data;
    tci_comm_t comm;
    tci_comm_init(&comm, data->context, data->tid, 1, 0);
    data->func(&comm, data->payload);
    tci_comm_destroy(&comm);
    return NULL;
}

int tci_parallelize_int(tci_thread_func_t func, void* payload, int nthread, int arity)
{
    tci_context_t* context;
    int ret = tci_context_init(&context, nthread, arity);
    if (ret != 0) return ret;

    pthread_t threads[nthread];
    tci_thread_data_t data[nthread];

    for (int i = 1;i < nthread;i++)
    {
        data[i].func = func;
        data[i].payload = payload;
        data[i].context = context;
        data[i].tid = i;

        int ret = pthread_create(&threads[i], NULL, tci_run_thread, &data[i]);
        if (ret != 0)
        {
            for (int j = i-1;j >= 0;j--) pthread_join(&threads[j], NULL);
        }
    }

    tci_comm_t comm0;
    tci_comm_init(&comm0, context, 0, 1, 0);
    func(&comm0, payload);

    for (int i = 1;i < nthread;i++)
    {
        pthread_join(&threads[i], NULL);
    }

    return tci_comm_destroy(&comm0);
}

#else

int tci_parallelize_int(tci_thread_func_t func, void* payload, int nthread, int arity)
{
    tci_comm_t comm;
    tci_comm_init_single(&comm);
    func(&comm, payload);
    tci_comm_destroy(&comm);
    return 0;
}

#endif

int tci_get_env(const char* env, int fallback)
{
    const char* str = getenv(env);
    //TODO: error handling for strtol
    if (str) return strtol(str, NULL, 10);
    return fallback;
}

int tci_parallelize(tci_thread_func_t func, void* payload, int nthread, int arity)
{
    if (nthread == 0)
    {
        nthread = tci_get_env("OMP_NUM_THREADS", 1);
    }

    if (nthread > 1)
    {
        return tci_parallelize_int(func, payload, nthread, arity);
    }
    else
    {
        tci_comm_t comm;
        tci_comm_init_single(&comm);
        func(&comm, payload);
        tci_comm_destroy(&comm);
        return 0;
    }
}

void tci_prime_factorization(int n, tci_prime_factors_t* factors)
{
    factors->n = n;
    factors->sqrt_n = (int)sqrt(n);
    factors->f = 2;
}

int tci_next_prime_factor(tci_prime_factors_t* factors)
{
    for (;factors->f <= factors->sqrt_n;factors->f++)
    {
        if (factors->n%factors->f == 0)
        {
            factors->n /= factors->f;
            return factors->f;
        }
    }

    if (factors->n != 1)
    {
        int tmp = factors->n;
        factors->n = 1;
        return tmp;
    }

    return 1;
}

void tci_partition_2x2(int nthread, int64_t work1, int64_t work2, int* nt1, int* nt2)
{
    tci_prime_factors_t factors;
    tci_prime_factorization(nthread, &factors);

    *nt1 = 1;
    *nt2 = 1;

    int f;
    while ((f = tci_next_prime_factor(&factors)) > 1)
    {
        if (work1 > work2)
        {
            work1 /= f;
            *nt1 *= f;
        }
        else
        {
            work2 /= f;
            *nt2 *= f;
        }
    }
}
