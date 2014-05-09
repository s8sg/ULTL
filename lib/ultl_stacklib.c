
/* get_stacksize() : To get the stack size
 * @node_head : start of the stack
 * @return : On success return stack size
 *           On failure return error code
 */ 
int get_stacksize(node_head *start){

   int size = 0;
   node_head *temp = NULL;
 
   if(start == NULL){
       ult_error("Invalid Argument : start is NULL");
 
       return -ESTK_INVALARG;      
   }
   temp = start;
   while(temp != NULL){
      size++;
      temp = NODE_NEXT(temp);
   }
   return size;
}

/* push_node() : Push node to a stack
 * @stack :      Start node of the stack
 * @new_node:    new node to push
 * @return:      On success current stack size
 *               On failure return negative error code
 */
int push_node(node_head *start, node_head* new_node){
   
   int ret = 0;

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

   ret = get_stacksize(start);
   return 0;
}

/* pop_node() :  Pop node to a stack
 * @start :      Start node of the stack
 * @return:      On success poped node
 *               On failure return NULL
 */
node_head *pop_node(node_head *start){

   node_head *ret = NULL;
   
   if(start == NULL){
      ult_error("Invalid argument: start is null");
      return NULL;
   }
   ret = start;
   start = NODE_NEXT(start);
   NODE_PREV(start) = NULL;
   RESET_NODE(ret);
   
   return ret->data;
}

