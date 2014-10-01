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
    int reg_time;
    int delay;
    alarm_handler_t alarm_func;
    void* alarm_func_arg;
} alarm;

alarm_list_t a_list;

int alarm_list_len(alarm_list_t a_list){
    return a_list->len;
}

alarm_id
set_alarm(int delay, alarm_handler_t alarm, void* arg, int reg_time ){
    alarm_t new_alarm;
    alarm_node_t new_node;
    alarm_node_t curr_hd;

    new_alarm = (alarm_t)register_alarm(delay,alarm, arg);
    new_alarm->reg_time = reg_time;

    new_node = (alarm_node_t)malloc(sizeof(alarm_node));
    if (!new_node){
        return NULL;
    }
    //initialize..
    new_node->alarm = new_alarm;
    new_node->next = NULL;
    curr_hd = a_list->head;

    if (a_list->len == 0){
        //make new node head since list was empty
        a_list->head = new_node;
    } else if (curr_hd->alarm->reg_time +
            curr_hd->alarm->delay - reg_time > delay){
        //make new node head since old head was later
        new_node->next = curr_hd;
        a_list->head = new_node;
    } else {
        //add new node to right place in sorted list
        while (curr_hd->next != NULL &&
                    curr_hd->next->alarm->reg_time +
                    curr_hd->next->alarm->delay - reg_time < delay){
           curr_hd = curr_hd->next; 
        }
        new_node->next = curr_hd->next;
        curr_hd->next = new_node;
    }
    a_list->len += 1;
    return new_alarm;
}

/* see alarm.h */
alarm_id
register_alarm(int delay, alarm_handler_t alarm, void *arg)
{
    alarm_t new_alarm;

    new_alarm = (alarm_t)malloc(sizeof(alarm));
    if (!new_alarm){
        return NULL;
    }

    //initialize...
    new_alarm->delay = delay;
    new_alarm->alarm_func = alarm;
    new_alarm->alarm_func_arg = arg;

    return new_alarm;
}

/* see alarm.h */
//assumptions: deregister always called on alarms that
//were actually registered before
int
deregister_alarm(alarm_id alarm)
{
    alarm_node_t curr_hd;
    alarm_node_t tmp;
    //if null alarm, return 0
    if (alarm == NULL){
        return 0;
    }
    //if list empty, alarm was executed
    if (a_list->len == 0){
        return 1;
    }
    curr_hd = a_list->head;
    //if head is alarm, free it and return 0
    if ( (alarm_id)(curr_hd->alarm) == alarm ){
        tmp = curr_hd->next;
        //free(curr_hd->alarm->alarm_func);
        //free(curr_hd->alarm->alarm_func_arg);
        free(curr_hd->alarm);
        free(curr_hd);

        a_list->head = tmp;
        a_list->len--;
        return 0;
    }
    while (curr_hd->next != NULL && (alarm_id)(curr_hd->next->alarm) != alarm){
        curr_hd = curr_hd->next;
    }
    //could not find, alarm was executed previously
    if (curr_hd->next == NULL){
        return 1;
    } else {
        tmp = curr_hd->next->next;
        free(curr_hd->next->alarm);
        free(curr_hd->next);
        curr_hd->next = tmp;
        a_list->len--;
        return 0;
    }
}

void execute_alarm(int sys_time){
    alarm_node_t curr_hd;
    alarm_node_t tmp;
    curr_hd = a_list->head;
    //iterate to find all alarms that need to be executed
    //free each node and alarm and func/func arg after execution
    while (curr_hd != NULL && curr_hd->alarm->reg_time +
                curr_hd->alarm->delay == sys_time){
        ( (curr_hd->alarm)->alarm_func )( (curr_hd->alarm)->alarm_func_arg);
        tmp = curr_hd;
        curr_hd = curr_hd->next;
        free(tmp->alarm);
        free(tmp);
        a_list->len--;
    }
    a_list->head = curr_hd;
    return;
}

alarm_list_t
init_alarm(){
    a_list = (alarm_list_t)malloc(sizeof(alarm_list));
    a_list->len = 0;
    a_list->head = NULL;
    return a_list;
}

/*
** vim: ts=4 sw=4 et cindent
*/
