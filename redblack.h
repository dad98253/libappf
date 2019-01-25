/********************************************************************************
 *                 _____                      _  ______ _____                   *
 *                /  ___|                    | | | ___ \  __ \                  *
 *                \ `--. _ __ ___   __ _ _ __| |_| |_/ / |  \/                  *
 *                 `--. \ '_ ` _ \ / _` | '__| __|    /| | __                   *
 *                /\__/ / | | | | | (_| | |  | |_| |\ \| |_\ \                  *
 *                \____/|_| |_| |_|\__,_|_|   \__\_| \_|\____/ Inc.             *
 *                                                                              *
 ********************************************************************************
 *                                                                              *
 *                        copyright 2017 by SmartRG, Inc.                       *
 *                               Santa Barbara, CA                              *
 *                                                                              *
 ****************************************************************************//**
 *
 * @file      redblack.h
 * @authors   charles.gelinas@smartrg.com
 *
 * @brief     Defines the Red-Black Tree APIs
 * @details
 *
 * @version   1.0
 *
 * @copyright 2017 by SmartRG, Inc.
 *******************************************************************************/

#ifndef _REDBLACK_H_
#define _REDBLACK_H_

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *                                                                             *
 *                                    INCLUDES                                 *
 *                                                                             *
 *******************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

/*******************************************************************************
 *                                                                             *
 *                                    DEFINES                                  *
 *                                                                             *
 *******************************************************************************/

/*******************************************************************************
 *                                                                             *
 *                                   TYPEDEFS                                  *
 *                                                                             *
 *******************************************************************************/

//! Red-Black tree operation results
typedef enum rb_result {
	RB_SUCCESS = 0,			//!< Operation succeeded
	RB_FAILURE = 1,			//!< Operation failed
} rb_result_e;

//! Red-Black tree walk return values
typedef enum rb_walk_returns {
	RB_WALK_BREAK = 0,
	RB_WALK_CONT  = 1,
} rb_walk_returns_e;

typedef enum rb_visit {
	RB_VISIT_PREORDER  = 0,
	RB_VISIT_POSTORDER = 1,
	RB_VISIT_ENDORDER  = 2,
	RB_VISIT_LEAF      = 3,
} rb_visit_e;

typedef enum rb_node_color {
	RB_BLACK = 0,
	RB_RED   = 1,
} rb_node_color_e;

typedef void rb_elem_t;

/**
 * @brief    Prototype of the comparison function  to be passed to rb_tree_create.
 * @details  It compares the two elements pKey & pEntry and returns an integer less than, equal to, or
 *           greater than zero accordingly as pKey is less than, equal to, or greater than pEntry.
 *
 * @param pKey    pointer to the comparator
 * @param pEntry  pointer to the element from the tree to compare with @p pKey
 *
 * @return  an integer less than, equal to, or greater than zero accordingly as pKey is less than, equal
 *          to, or greater than pEntry.
 */
typedef int32_t (*rb_compare_cb)(rb_elem_t *pKey, rb_elem_t *pEntry);

/**
 * @brief    prototype of the callback invoked from rb_tree_destroy and rb_tree_drain
 * @details  The first arg (elem) is the application node (key). The second is an optional, user defined data
 */
typedef int32_t (*rb_key_free_cb)(rb_elem_t *pElement, void *pData);

typedef int32_t (*rb_walk_cb)(rb_elem_t *pElem, rb_visit_e visit, uint32_t level, void *pData, void *pOut);
typedef int32_t (*rb_walk_inorder_cb)(rb_elem_t *pElem, uint32_t level, void *pData, void *pOut);

typedef struct rb_node {
	struct rb_node		*pLeft;		//!< pointer to left child node
	struct rb_node		*pRight;	//!< pointer to right child node
	struct rb_node		*pUp;		//!< pointer to parent
	rb_node_color_e		 color;		//!< node colour
	rb_elem_t			*pKey;		//!< pointer to the key
} rb_node_t;

typedef struct rb_tree {
	rb_compare_cb		 fCompare;	//!< Comparison function for this tree
	pthread_mutex_t		 mutex;		//!< PTHREAD mutex
	rb_node_t			 root;
	rb_node_t			*pRoot;		//!< Pointer to the root node
	uint32_t			 count;
	size_t				 offset;	//!< Offset of rbnode in the user structure
} rb_tree_t;

/*******************************************************************************
 *                                                                             *
 *                              GLOBAL PROTOTYPES                              *
 *                                                                             *
 *******************************************************************************/

void rb_tree_init(rb_tree_t *pTree, size_t offset, rb_compare_cb fCompareCB);

rb_tree_t * rb_tree_create(size_t offset, rb_compare_cb fCompareCB);

void rb_tree_delete(rb_tree_t *pTree);

void rb_tree_destroy(rb_tree_t *pTree, rb_key_free_cb fFreeCB, void *pData);

void rb_tree_flush(rb_tree_t *pTree, rb_key_free_cb fFreeCB, void *pData);

rb_result_e __rb_tree_add(rb_tree_t *pTree, rb_elem_t *pElement);
rb_result_e rb_tree_add(rb_tree_t *pTree, rb_elem_t *pElement);

rb_elem_t * __rb_tree_remove(rb_tree_t *pTree, rb_elem_t *pElement);
rb_elem_t * rb_tree_remove(rb_tree_t *pTree, rb_elem_t *pKey);

rb_elem_t * __rb_tree_get(rb_tree_t *pTree, rb_elem_t *pKey, rb_compare_cb fCompareCB);
rb_elem_t * rb_tree_get(rb_tree_t *pTree, rb_elem_t *pKey, rb_compare_cb fCompareCB);

rb_elem_t * __rb_tree_get_first(rb_tree_t *pTree);
rb_elem_t * rb_tree_get_first(rb_tree_t *pTree);

rb_elem_t * __rb_tree_get_next(rb_tree_t *pTree, rb_elem_t *pKey, rb_compare_cb fCompareCB);
rb_elem_t * rb_tree_get_next(rb_tree_t *pTree, rb_elem_t *pKey, rb_compare_cb fCompareCB);

rb_elem_t * __rb_tree_get_next_from_node(rb_tree_t *pTree, rb_elem_t *pNode);
rb_elem_t * rb_tree_get_next_from_node(rb_tree_t *pTree, rb_elem_t *pNode);

void __rb_tree_walk(rb_tree_t *pTree, rb_walk_cb fActionCB, void *pData, void *pOut);
void rb_tree_walk(rb_tree_t *pTree, rb_walk_cb fActionCB, void *pData, void *pOut);

void __rb_tree_walk_inorder(rb_tree_t *pTree, rb_walk_inorder_cb fActionCB, void *pData, void *pOut);
void rb_tree_walk_inorder(rb_tree_t *pTree, rb_walk_inorder_cb fActionCB, void *pData, void *pOut);

rb_elem_t * __rb_tree_step(rb_tree_t *pTree, rb_elem_t *pElement, int dir);
rb_elem_t * rb_tree_step(rb_tree_t *pTree, rb_elem_t *pElement, int dir);

/*******************************************************************************
 *                                                                             *
 *                               GLOBAL VARIABLES                              *
 *                                                                             *
 *******************************************************************************/

/*******************************************************************************
 *                                                                             *
 *                                   MACROS                                    *
 *                                                                             *
 *******************************************************************************/

//! Retrieve the offset of a given member of a given type
#define RB_OFFSETOF(_type, _member)				((size_t)(&(((_type*) 0)->_member)))

/*******************************************************************************
 *                                                                             *
 *                                   INLINES                                   *
 *                                                                             *
 *******************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* _REDBLACK_H_ */
