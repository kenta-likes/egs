#include <stdio.h>
#include <stdlib.h>

#include "interrupts.h"
#include "alarm.h"
#include "minithread.h"
#include "queue.h"

typedef struct alarm_node{
    alarm_t alarm;
    alarm_node_t next;
} alarm_node;

typedef struct alarm_list{
    int len;
    alarm_node_t head;
} alarm_list;

typedef struct alarm{
    int alarm_time;
    alarm_handler_t alarm_func;
    void* alarm_func_arg;
} alarm;

alarm_list_t a_list;

/* see alarm.h */
alarm_id
register_alarm(int delay, alarm_handler_t alarm, void *arg)
{
    alarm_t new_alarm;
    alarm_node_t new_node;
    alarm_node_t curr_hd;

    new_alarm = (alarm_t)malloc(sizeof(alarm));
    if (!new_alarm){
        return NULL;
    }
    //initialize...
    new_alarm->alarm_time = delay;
    new_alarm->alarm_func = alarm;
    new_alarm->alarm_func_arg = arg;

    new_node = (alarm_node_t)malloc(sizeof(alarm_node));
    if (!new_node){
        return NULL;
    }
    //initialize..
    new_node->alarm = new_alarm;
    if (a_list->len == 0){
        //make new node head since list was empty
        a_list->head = new_node;
    } else if (a_list->head->alarm->alarm_time > delay){
        //make new node head since old head was later
        new_node->next = a_list->head;
        a_list->head = new_node;
    } else {
        //add new node to right place in sorted list
        curr_hd = a_list->head;
        while (curr_hd->next != NULL &&
                curr_hd->next->alarm->alarm_time < delay){
           curr_hd = curr_hd->next; 
        }
        new_node->next = curr_hd->next;
        curr_hd->next = new_node;
    }
    return new_alarm;
}

/* see alarm.h */
int
deregister_alarm(alarm_id alarm)
{
    free((alarm_t)alarm);
    return 0;
}

void execute_alarm(int sys_time){
    alarm_node_t curr_hd;
    curr_hd = a_list->head;
    //iterate to find all alarms that need to be executed
    while (curr_hd != NULL && curr_hd->alarm->alarm_time < sys_time){
        ( (curr_hd->alarm)->alarm_func )( (curr_hd->alarm)->alarm_func_arg);
        free(curr_hd->alarm->alarm_func);
        free(curr_hd->alarm->alarm_func_arg);
        free(curr_hd->alarm);
        free(curr_hd);
    }
    a_list->head = curr_hd;
    return;
}

void init_alarm(){
    a_list = (alarm_list_t)malloc(sizeof(alarm_list));
    a_list->len = 0;
    a_list->head = NULL;
}

/*
** vim: ts=4 sw=4 et cindent
*/
