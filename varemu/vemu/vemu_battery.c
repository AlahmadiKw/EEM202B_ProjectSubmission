
/*
 * Battery model 
 * Mohammad Mohammad 
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "vemu_battery.h"
#include "vemu.h"
#include "qemu-common.h"
#include "qemu/timer.h"
#include "vemu-cycles.h"

#ifdef  VEMU

/* global variables */ 
double glob_voltage = 3.75; 

struct Bat_param bat_param; 
struct Bat_data bat_data; 
Array steps;

/* battery initial conditions */
int sum = 0; 
int numLoads = 0; 
double charge = 0;
int bat_flag = 0; 
double L = -1;

void initArray(Array *a, uint64_t initialSize) {
    a->array = (struct Step *)malloc(initialSize * sizeof(struct Step));
    a->used = 0;
    a->size = initialSize;
}
void insertArray(Array *a, struct Step element) {
    if (a->used == a->size) {
        a->size *= 2;
        a->array = (struct Step *)realloc(a->array, a->size * sizeof(struct Step));
    }
    a->array[a->used++] = element;
}
void freeArray(Array *a) {
    free(a->array);
    a->array = NULL;
    a->used = a->size = 0;
}


void loadParam(struct Bat_param *params){

    FILE* configData;

    if ((configData = fopen ("vemu_config.txt", "r")) == NULL) {
        printf("\n\n*** ERROR: fail opening configuration data file...\n\n");
        perror ("ERROR");
    } 

    double alpha, beta, delta, voltage; 
    int num_terms; 
    fscanf(configData, "%lf\n", &alpha);
    fscanf(configData, "%lf\n", &beta);
    fscanf(configData, "%d\n", &num_terms);
    fscanf(configData, "%lf\n", &delta);
    fscanf(configData, "%lf\n", &voltage);

    params->alpha = alpha; 
    params->beta = beta;
    params->num_terms = num_terms; 
    params->delta = delta; 
    params-> voltage = voltage; 

    glob_voltage = voltage; 

    if (fclose(configData) == EOF) {
        printf("\n\n*** ERROR: fail closing configuration data file...\n\n");
        perror ("ERROR");
    }  
}

struct Step createStep(uint64_t index, double current, double duration, double start) {
    struct Step step; 
    step.stepIndex = index;
    step.currentLoad = current;
    step.loadDuration = duration;
    step.startTime = start;
    return step;
}

double A_func(double ln, double tk_d, double tk){

    int m;
    double x = 0;
    double num_terms  = bat_param.num_terms;
    for (m = 1; m <= num_terms; m++) {
        x = x + (exp(-bat_param.beta* bat_param.beta*m*m*(ln-tk_d)) - exp(-bat_param.beta*bat_param.beta*m*m*(ln-tk)))/(bat_param.beta*bat_param.beta*m*m);
    }
    return x;
}

/* alternative model */

double lt = 0;
double eplsilon = 0;
double ch_old=0;
double ch =0;
struct Step old_step;


struct Bat_data compute_new(struct Step step){
    static int count = 0;
    struct Bat_data bat_data;

    loadParam(&bat_param);                                                                                                                              

    bat_data.bat_param = bat_param;

    double alpha  = bat_param.alpha;

    double landa;
    double thrd_term;
    double temp;
    double temp2;

    double current = step.currentLoad;
    double duration = step.loadDuration;
    double startTime = step.startTime;

    if (count == 0){

        ch_old = 0;
        lt = lt + current*duration;
        count++;

        eplsilon = A_func(startTime+duration, startTime+duration, startTime)/A_func(startTime, startTime+duration,startTime);
        landa = eplsilon;                                                                                                                               
        temp = A_func(startTime+duration, startTime+duration, startTime);
        thrd_term = 2 * current * temp;
        temp2 = lt + current*duration;
        ch = temp2 + thrd_term;

        bat_data.results.soc = (1-ch/alpha) * 100;
        bat_data.results.charge = ch;
        old_step = step;
        return bat_data;

    } else if (count == 1) {

        double oldduration = old_step.loadDuration;
        double oldstartTime = old_step.startTime;

        eplsilon = A_func(startTime+duration, oldstartTime+oldduration, oldstartTime)/A_func(startTime, oldstartTime+oldduration,oldstartTime);
        landa = eplsilon;

        temp = A_func(startTime+duration, startTime+duration, startTime);

        thrd_term = 2 * current * temp;
        temp2 = lt + current*duration;

        ch = temp2 + landa * (ch_old - lt) + thrd_term;
        ch_old = ch;

        lt = lt + current*duration; 



        bat_data.results.soc = (1-ch/alpha) * 100;
        bat_data.results.charge = ch;

        if (ch > alpha) {
            ch = alpha;
            bat_data.results.soc = (1-ch/alpha) * 100;
            bat_data.results.charge = ch;
            count++;

        }

        old_step = step;
        return bat_data;
    } else if (count == 2){
        printf("battery exhausted, available charge = 0\n");
        ch = alpha;
        bat_data.results.soc = (1-ch/alpha) * 100;
        bat_data.results.charge = ch;
        return bat_data;
    }
    return bat_data;  
}



#endif
