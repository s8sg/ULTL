

#define NODE_NEXT(node) (node->next)
#define NODE_PREV(node) (node->prev)
#define NODE_DATA(node) (node->data)

struct node_head {
    struct node_head *data;
    struct node_head *next;
    struct node_head *prev;
};

typedef struct node_head node_head;

node_head *create_node(void*);

int delete_node(node_head*, void**);
