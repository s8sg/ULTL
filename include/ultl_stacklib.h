

/* push_node() : Push node to a stack
 * @stack :      Start node of the stack
 * @new_node:    new node to push
 * @return:      On success current stack
 *               On failure return negative error code
 */
int push_node(node_head *start, node_head* new_node){
   
   if(start == NULL){
      ult_error("Invalid argument: start is null");
      return -ESTK_INVALARG;
   }
   if(new_node == NULL){
      ult_error("Invalid argument: node is null");
      return -ESTK_INVALARG;
   }
   NODE_NEXT(new_node) = start;
   NODE_PREV(start) = new_node;
   return 0;
}

/* push_node() : Push node to a stack
 * @start :      Start node of the stack
 * @new_node:    new node to push
 * @return:      On success current stack
 *               On failure return negative error code
 */
int push_node(node_head *start, node_head* new_node){
   
   if(start == NULL){
      ult_error("Invalid argument: start is null");
      return -ESTK_INVALARG;
   }
   if(new_node == NULL){
      ult_error("Invalid argument: node is null");
      return -ESTK_INVALARG;
   }
   NODE_NEXT(new_node) = start;
   NODE_PREV(start) = new_node;
   return 0;
}

