#include <stdlib.h>
#include <ultl_listlib.h>

/* create_node(): create a node of the list with next and prev as 
 *                NULL and data as passed
 * @data:         The data that will be hold, could be NULL 
 * @return:       On success new node_head type object
 *                On failure return NULL
 */ 
node_head *create_node(void *data) {
    
    node_head *temp_node = NULL;
    
    /* allocate memory for new node */   
    temp_node = (temp_node*)malloc(NODE_HEAD_SIZE);
    if(temp_node == NULL) {
       ultl_error("Memory allocation failed");
       return NULL;
    }
    NODE_NEXT(temp_node) = NULL;
    NODE_PREV(temp_node) = NULL;
    NODE_DATA(temp_node) = data;

    return temp_node;
}

/* delete_node(): remove a node from a list, and set data on data_ptr      
 * @node:         The node to remove
 * @data_ptr:     Set the return data on data_ptr 
 * @return:       On success return 0
 *                On failure return the error code
 */ 
int delete_node(node_head* node, void **data_ptr){

    int data_flag = 0;

    if(data_ptr != NULL) {      
      if(*data_ptr != NULL) {
         data_flag = 1;
      }
    }
    if(node == NULL){
        ultl_error("Invalid argument: node is NULL");
        return -EDEL_INVALARG; 
    }
    if(data_flag == 1){
        *data_ptr = NODE_DATA;
    }
    free(node);
    return 0;
}

/* add_node(): Add new node to a specified position 
 * @c_node:    Address of the node where to add
 * @n_node:    Address of the node to be added, could be NULL 
 * @position:  Position where the data would be added (L_NEXT, L_PREV)
 * @return  :  On success return 0
 *             On failure return the error code
 */
int add_node(node_head* c_node, node_head* n_node, int position){
   
   /* check if c_node is invalid */
   if(c_node == NULL) {
       ultl_error("Invalid argument: main node NULL");
       return -LADD_INVALARG; 
   }
   /* check position and add to specified position*/
   switch(position){
       case L_NEXT:
       NODE_NEXT(c_node) = n_node;
       break;

       case L_PREV:
       NODE_PREV(c_node) = n_node;
       break;

       default:
       ultl_error("Invalid argument: position invalid");
       return -LADD_INVALARG;
   }   
   return 0;
}



