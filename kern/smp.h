struct smp_data{
    int num_cpus;
};

void smp_data_init(void);
int get_numcpus(void);
int get_current_cpu(void);
int smp_init(void);
