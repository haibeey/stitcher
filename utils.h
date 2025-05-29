#ifdef __cplusplus
extern "C" {
#endif


int get_cpus_count();
int clamp(int value, int min, int max) ;
int min(int a , int b);
int max(int a , int b);
int reflect_index(int i,int n);

#ifdef __cplusplus
}
#endif
