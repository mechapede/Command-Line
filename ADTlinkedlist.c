/* Linkedlist implementation code */

#include <stdio.h>
#include <assert.h>

#include "ADTlinkedlist.h"

/* Initiate the linkedlist structure to safe default values */
void adtInitiateLinkedList(ADTlinkedlist * list){
    list->num =0;
    list->head = NULL;
}

/*Initiate the structure of a node to safe values with val set */
void adtInitiateLinkedNode(ADTlinkednode * node, void * val){
    node->val = val;
    node->next = NULL;
}

/*Add node to linked list. Returns -1 or failure. 0 otherwise */
int adtAddLinkedNode(ADTlinkedlist * list, ADTlinkednode * node, int index){
    assert(list);
    if( index > list->num){
        return -1;
    }else if( index < 0 ){
        return -1;
    }
    
    if( index == 0 ){
        node->next = list->head;
        list->head = node;
        list->num += 1;
        return 0;
    }
    
    ADTlinkednode * link = list->head;
    int i;
    for( i=0; i < index; i++){
        if(link->next == NULL) return -1; //corruption of list 
        link = link->next;
    } 
    
    node->next = link->next;
    link->next = node;
    list->num += 1;
    
    return 0;
}

/* Find the index of a value in linked list if it exsists. Must provide compare function. 
 * Returns -1 if not found 
 * */
ADTlinkednode * adtPeakLinkedNode(ADTlinkedlist * list, int index){
    assert(list);
    if( index >= list->num){
        return NULL;
    }else if(index < 0){
        return NULL;
    }
    
    ADTlinkednode * link = list->head;
    int i;
    for( i=0; i < index; i++){
        if(link->next == NULL) return NULL; //corruption of list 
        link = link->next;
    } 
    
    return link;
    
}

/* Get node from list, freeing will cauce corruption in the list 
 * Returns NULL on failure*/
ADTlinkednode * adtPopLinkedNode(ADTlinkedlist * list, int index){
    assert(list);
    if( index >= list->num){
        return NULL;
    }else if( index < 0){
        return NULL;
    }
    
    ADTlinkednode * link = list->head;
    
    if(index == 0){
        list->head = link->next;
        link->next = NULL;
        list->num -= 1;
        return link;
    }
    
    int i;
    for( i=0; i < index - 1; i++){
        if(link->next == NULL) return NULL; //corruption of list 
        link = link->next;
    } 
    
    ADTlinkednode * popped = link->next;
    link->next =(link->next)->next;
    popped->next = NULL;//defensive
    list->num -= 1;
    
    return popped;
    
    
}

/*Remove node from list. Caller responsible for free. 
 * Returns NULL on failure*/
int adtFindLinkedValue(ADTlinkedlist * list, void * val, int (*compare)(void * val1, void * val2) ){
    assert(list);
    if (list->num == 0){
        return -1;
    }
    
    ADTlinkednode * node = list->head;
    
    int index = 0;
    
    while(node){
        if( compare( node->val, val) ) return index;
        node = node->next;
        index++;
    }
    
    return -1;
    
    
}
