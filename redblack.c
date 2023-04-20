
//  Where are these routines used???  -jck


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
 *                         copyright 2017 by SmartRG, Inc.                      *
 *                               Santa Barbara, CA                              *
 *                                                                              *
 ***************************************************************************//***
 *
 * @file      redblack.c
 * @authors   charles.gelinas@smartrg.com
 *
 * @brief     Implementation of a Red-Black Tree
 * @details   ...
 *
 * @version   1.0
 *
 * @copyright 2017 SmartRG, Inc.
 *******************************************************************************/

/*******************************************************************************
 *                                                                             *
 *                                    INCLUDES                                 *
 *                                                                             *
 *******************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <netdb.h>
#include <syslog.h>
#include <ifaddrs.h>

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>

#include "redblack.h"

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

/*******************************************************************************
 *                                                                             *
 *                               LOCAL PROTOTYPES                              *
 *                                                                             *
 *******************************************************************************/

// Search for and if not found and insert is true, will add a new node in.
static rb_node_t * rb_traverse(bool insert, rb_elem_t *pKey, rb_tree_t *pTree, rb_compare_cb fCompare);

//! Destroy all the elements blow us in the tree only useful as part of a complete tree destroy.
static void rb_destroy(rb_tree_t *pTree, rb_node_t *pNode, rb_key_free_cb fFreeCB, void *pData);

//! Delete the node z, and free up the space
static void rb_delete(rb_node_t **ppRoot, rb_node_t *pNode, rb_tree_t *pTree);

//! Restore the reb-black properties after a delete
static void rb_delete_fix(rb_node_t **ppRoot, rb_node_t *pNode, rb_tree_t *pTree);

static rb_walk_returns_e rb_walk(rb_tree_t *pTree, rb_node_t *pNode, rb_walk_cb fAction, void *pData, uint32_t level, void *pOut);

static rb_walk_returns_e rb_walk_inorder(rb_tree_t *pTree,
											rb_node_t *pNode,
											rb_walk_inorder_cb fAction,
											void *pData,
											uint32_t level,
											void *pOut);

static void rb_rotate_left(rb_node_t **ppRoot, rb_node_t *pNode, rb_tree_t *pTree);

static void rb_rotate_right(rb_node_t **ppRoot, rb_node_t *pNode, rb_tree_t *pTree);

//!  Return a pointer to the smallest key greater than pNode
static rb_node_t * rb_successor(rb_node_t *pNode, rb_tree_t *pTree);

/*******************************************************************************
 *                                                                             *
 *                               LOCAL VARIABLES                               *
 *                                                                             *
 *******************************************************************************/

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

//! Retrieve the root node of an RB tree
#define RB_ROOT(_pTree)							(&(_pTree)->root)

//! Retrieve the address of the elements associated with a node
#define RB_ELEMENT(_pTree, _pNode)				(rb_elem_t *)(((uint8_t *) (_pNode)) - (_pTree)->offset)

//! Retrieve the address of the node associated with an element
#define RB_NODE(_pTree, _pElement)				(rb_node_t *)(((uint8_t *) (_pElement)) + (_pTree)->offset)

/*******************************************************************************
 *                                                                             *
 *                                IMPLEMENTATION                               *
 *                                                                             *
 *******************************************************************************/

/**
 * @brief    Initializes an instance of a RB Tree.
 * @details
 *
 * @param pTree       pointer to the tree we are initializing
 * @param offset      offset of the @ref rb_node_t within the element structure
 * @param fCompareCB  pointer to the callback use to compare elements in the tree
 *
 * @author  charles.gelinas@smartrg.com
 */
void rb_tree_init(rb_tree_t *pTree, size_t offset, rb_compare_cb fCompareCB)
{
	rb_node_t  *pRoot = NULL;

	if (pTree) {
		pRoot = RB_ROOT(pTree);

		pRoot->color  = RB_BLACK;
		pRoot->pLeft  = pRoot;
		pRoot->pRight = pRoot;
		pRoot->pUp    = pRoot;
		pRoot->pKey   = NULL;

		pTree->fCompare = fCompareCB;
		pTree->pRoot    = pRoot;
		pTree->count    = 0;

		// create mutex
		pthread_mutex_init(&pTree->mutex, NULL);
		pTree->offset = offset;
	}
}

/**
 * @brief    Creates an instance of an RB tree
 * @details
 *
 * @param offset      offset of the @ref rb_node_t within the element structure
 * @param fCompareCB  pointer to the callback use to compare elements in the tree
 *
 * @return  a pointer to the newly allocated and initialized tree or NULL.
 * @author  charles.gelinas@smartrg.com
 */
rb_tree_t * rb_tree_create(size_t offset, rb_compare_cb fCompareCB)
{
	rb_tree_t  *pTree = NULL;

	pTree = calloc(1, sizeof(rb_tree_t));
	if (!pTree) {
		return (NULL);
	}

	rb_tree_init(pTree, offset, fCompareCB);
	return (pTree);
}

/**
 * @brief    Deletes an instance of a RB Tree
 * @details
 *
 * @param pTree  pointer to the tree to delete
 *
 * @author  charles.gelinas@smartrg.com
 */
void rb_tree_delete(rb_tree_t *pTree)
{
	if (pTree) {
		rb_destroy(pTree, pTree->pRoot, NULL, 0);
		pthread_mutex_destroy(&pTree->mutex);
		free(pTree);
	}
}

/**
 * @brief    Deletes an instance of a RB Tree
 * @details  This function ensures that the memory allocated for the keys (by the
 *           application) gets freed.
 *
 * @param pTree    pointer to the tree we are flushing
 * @param fFreeCB  pointer to the callback for each node we are destroying
 * @param pData    transparent pointer to the data passed to @p fFreeCB
 *
 * @author  charles.gelinas@smartrg.com
 */
void rb_tree_destroy(rb_tree_t *pTree, rb_key_free_cb fFreeCB, void *pData)
{
	if (pTree) {
		rb_destroy(pTree, pTree->pRoot, fFreeCB, pData);
		pthread_mutex_destroy(&pTree->mutex);
		free(pTree);
	}
}

/**
 * @brief    Deletes the nodes alone from an RB tree.
 * @details
 *
 * @param pTree    pointer to the tree we are flushing
 * @param fFreeCB  pointer to the callback for each node we are flushing
 * @param pData    transparent pointer to the data passed to @p fFreeCB
 *
 * @author  charles.gelinas@smartrg.com
 *
 * @note  Does not delete the tree.
 */
void rb_tree_flush(rb_tree_t *pTree, rb_key_free_cb fFreeCB, void *pData)
{
	if (pTree && fFreeCB) {
		rb_destroy(pTree, pTree->pRoot, fFreeCB, pData);

		pTree->count = 0;
		pTree->pRoot = RB_ROOT(pTree);
	}
}

/**
 * @brief    Adds an element to an RB tree
 * @details
 *
 * @param pTree     pointer to the RB tree we are adding to
 * @param pElement  pointer to the element to add
 *
 * @return  @ref RB_SUCCESS or @ref RB_FAILURE.
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This function is not thread safe. Use the @ref rb_tree_add()
 *        function if concurrency is an issue.
 */
rb_result_e __rb_tree_add(rb_tree_t *pTree, rb_elem_t *pElement)
{
	rb_node_t  *pNode  = NULL;

	// Make sure we have a valid tree and element pointer
	if (pTree && pElement) {
		// Traverse the tree and insert
		pNode = rb_traverse(true, pElement, pTree, NULL);
		if (pNode != RB_ROOT(pTree)) {
			pTree->count++;
			return (RB_SUCCESS);
		}
	}
	return (RB_FAILURE);
}

/**
 * @brief    Adds an element to an RB tree
 * @details
 *
 * @param pTree     pointer to the RB tree we are adding to
 * @param pElement  pointer to the element to add
 *
 * @return  @ref RB_SUCCESS or @ref RB_FAILURE.
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This is a thread safe implementation.
 */
rb_result_e rb_tree_add(rb_tree_t *pTree, rb_elem_t *pElement)
{
	rb_result_e  result = RB_FAILURE;

	// Make sure we have a valid tree and element pointer
	if (pTree && pElement) {
		pthread_mutex_lock(&pTree->mutex);
		{
			result = __rb_tree_add(pTree, pElement);
		}
		pthread_mutex_unlock(&pTree->mutex);
	}

	return (result);
}

/**
 * @brief    Removes an element from an RB tree
 * @details
 *
 * @param pTree     pointer to the RB tree we are removing from
 * @param pElement  pointer to the element to remove
 *
 * @return  a pointer to the element being removed on success.
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This function is not thread safe. Use the @ref rb_tree_remove()
 *        function if concurrency is an issue.
 */
rb_elem_t * __rb_tree_remove(rb_tree_t *pTree, rb_elem_t *pElement)
{
	rb_node_t  *pNode = NULL;
	rb_elem_t  *pElem = NULL;

	if (pTree != NULL) {
		pNode = rb_traverse(false, pElement, pTree, NULL);
		if (pNode != RB_ROOT(pTree)) {
			pElem = RB_ELEMENT(pTree, pNode);

			rb_delete(&pTree->pRoot, pNode, pTree);

			pTree->count--;
		}
	}
	return (pElem);
}

/**
 * @brief    Removes an element from an RB tree
 * @details
 *
 * @param pTree     pointer to the RB tree we are removing from
 * @param pElement  pointer to the element to remove
 *
 * @return  a pointer to the element being removed on success.
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This is a thread safe implementation.
 */
rb_elem_t * rb_tree_remove(rb_tree_t *pTree, rb_elem_t *pElement)
{
	rb_elem_t  *pElem = NULL;

	if (pTree != NULL) {
		pthread_mutex_lock(&pTree->mutex);
		{
			pElem = __rb_tree_remove(pTree, pElement);
		}
		pthread_mutex_unlock(&pTree->mutex);
	}
	return (pElem);
}

/**
 * @brief    Find the RB tree node matching the given key
 * @details
 *
 * @param pTree       pointer to the tree we are searching in
 * @param pKey        pointer to the key use to compare for matching
 * @param fCompareCB  pointer to the callback function use to compare
 *
 * @return  a pointer to the element if one is found matching or NULL
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This function is not thread safe. Use the @ref rb_tree_get()
 *        function if concurrency is an issue.
 */
rb_elem_t * __rb_tree_get(rb_tree_t *pTree, rb_elem_t *pKey, rb_compare_cb fCompareCB)
{
	rb_node_t  *pNode    = NULL;
	rb_elem_t  *pElement = NULL;

	if (pTree != NULL) {
		pNode = RB_ROOT(pTree);
		if (pTree->pRoot != RB_ROOT(pTree)) {
			pNode = rb_traverse(false, pKey, pTree, fCompareCB);
		}

		if (pNode != RB_ROOT(pTree)) {
			pElement = RB_ELEMENT(pTree, pNode);
		}
	}

	return (pElement);
}

/**
 * @brief    Find the RB tree node matching the given key
 * @details
 *
 * @param pTree       pointer to the tree we are searching in
 * @param pKey        pointer to the key use to compare for matching
 * @param fCompareCB  pointer to the callback function use to compare
 *
 * @return  a pointer to the element if one is found matching or NULL
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This is a thread safe implementation.
 */
rb_elem_t * rb_tree_get(rb_tree_t *pTree, rb_elem_t *pKey, rb_compare_cb fCompareCB)
{
	rb_elem_t  *pElement = NULL;

	if (pTree != NULL) {
		pthread_mutex_lock(&pTree->mutex);
		{
			pElement = __rb_tree_get(pTree, pKey, fCompareCB);
		}
		pthread_mutex_unlock(&pTree->mutex);
	}

	return (pElement);
}

/**
 * @brief
 * @details
 *
 * @param pTree  pointer to the RB tree
 *
 * @return
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This function is not thread safe. Use the @ref rb_tree_get_first()
 *        function if concurrency is an issue.
 */
rb_elem_t * __rb_tree_get_first(rb_tree_t *pTree)
{
	rb_node_t  *pNode    = NULL;
	rb_elem_t  *pElement = NULL;

	if (pTree != NULL) {
		pNode = pTree->pRoot;
		if (pNode != RB_ROOT(pTree)) {
			while (pNode->pLeft != RB_ROOT(pTree)) {
				pNode = pNode->pLeft;
			}

			pElement = RB_ELEMENT(pTree, pNode);
		}
	}

	return (pElement);
}

/**
 * @brief
 * @details
 *
 * @param pTree  pointer to the RB tree
 *
 * @return
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This is a thread safe implementation.
 */
rb_elem_t * rb_tree_get_first(rb_tree_t *pTree)
{
	rb_elem_t  *pElement = NULL;

	if (pTree != NULL) {
		pthread_mutex_lock(&pTree->mutex);
		{
			pElement = __rb_tree_get_first(pTree);
		}
		pthread_mutex_unlock(&pTree->mutex);
	}

	return (pElement);
}

/**
 * @brief    Retrieve the next node from the tree matching @p pKey
 * @details
 *
 * @param pTree  pointer to the RB tree
 * @param pKey   pointer to the element we use to compare with
 * @param fCompareCB  pointer to the callback use to compare a tree node with @p pKey
 *
 * @return  a pointer to the next matching element on NULL.
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This function is not thread safe. Use the @ref rb_tree_get_next()
 *        function if concurrency is an issue.
 */
rb_elem_t * __rb_tree_get_next(rb_tree_t *pTree, rb_elem_t *pKey, rb_compare_cb fCompareCB)
{
	bool        found     = false;
	int32_t     cmp       = 0;
	rb_node_t  *pNode     = NULL;
	rb_node_t  *pPrevNode = NULL;

	if (pTree == NULL) {
		return (NULL);
	}

	pPrevNode = RB_ROOT(pTree); // points to the parent of pNode
	pNode     = pTree->pRoot;

	if (pNode == RB_ROOT(pTree)) {
		// No node present in the pTree.
		return (NULL);
	}

	fCompareCB = ((fCompareCB == NULL) ? (pTree->fCompare) : fCompareCB);

	while (!found && (pNode != RB_ROOT(pTree))) {
		pPrevNode = pNode;

		cmp = (*fCompareCB)(pKey, RB_ELEMENT(pTree, pNode));
		if (cmp < 0) {
			pNode = pNode->pLeft;
		} else if (cmp > 0) {
			pNode = pNode->pRight;
		} else {
			found = true;
		}
	}

	if (pNode == RB_ROOT(pTree)) {
		pNode = pPrevNode;
	}

	cmp = (*fCompareCB)(pKey, RB_ELEMENT(pTree, pNode));
	if (found || (cmp > 0)) {
		pNode = rb_successor(pNode, pTree);
	}

	return (((pNode == RB_ROOT(pTree)) ? NULL : RB_ELEMENT(pTree, pNode)));
}

/**
 * @brief    Retrieve the next node from the tree matching @p pKey
 * @details
 *
 * @param pTree  pointer to the RB tree
 * @param pKey   pointer to the element we use to compare with
 * @param fCompareCB  pointer to the callback use to compare a tree node with @p pKey
 *
 * @return  a pointer to the next matching element on NULL.
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This is a thread safe implementation.
 */
rb_elem_t * rb_tree_get_next(rb_tree_t *pTree, rb_elem_t *pKey, rb_compare_cb fCompareCB)
{
	rb_elem_t  *pElement = NULL;

	if (pTree != NULL) {
		pthread_mutex_lock(&pTree->mutex);
		{
			pElement = __rb_tree_get_next(pTree, pKey, fCompareCB);
		}
		pthread_mutex_unlock(&pTree->mutex);
	}

	return (pElement);
}

/**
 * @brief    Given a particular node in a tree get the next one.
 * @details  We are assuming the node is either NULL or the
 *           rb_node_t of a structure within the tree.
 *
 * @param pTree  pointer to the tree
 * @param pNode  pointer to the node we are getting the next one from
 *
 * @return  a pointer to the next node or NULL
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This function is not thread safe. Use the @ref rb_tree_get_next_from_node()
 *        function if concurrency is an issue.
 */
rb_elem_t * __rb_tree_get_next_from_node(rb_tree_t *pTree, rb_elem_t *pNode)
{
	rb_elem_t  *pNext  = NULL;
	rb_node_t  *pNodeX = NULL;

	//
	// If we are not given a node in the pTree then we walk down the pLeft side
	// of the pTree as far as we can.  Otherwise we can find the successor.
	//
	if (pNode == NULL) {
		pNodeX = pTree->pRoot;

		if (pNodeX != RB_ROOT(pTree)) {
			// Walk down the pLeft side of the pTree.
			while (pNodeX->pLeft != RB_ROOT(pTree)) {
				pNodeX = pNodeX->pLeft;
			}

			//
			// Get the pointer to the beginning of the structure based on the
			// offset of the rb_node.
			//
			pNext = RB_ELEMENT(pTree, pNodeX);
		}
	} else {
		// We are starting from a node within the pTree.
		pNodeX = rb_successor(pNode, pTree);

		//
		// If we have a successor then get the pointer to the beginning of the
		// structure based on the offset of the rb_node.
		//
		if (pNodeX != RB_ROOT(pTree)) {
			pNext = RB_ELEMENT(pTree, pNodeX);
		}
	}

	return (pNext);
}

/**
 * @brief    Given a particular node in a tree get the next one.
 * @details  We are assuming the node is either NULL or the
 *           rb_node_t of a structure within the tree.  The mutex
 *           lock only guards against the tree changing while we
 *           are within the function.  There is no guarantee once
 *           we get the next node that the node itself has not
 *           been removed by another thread.
 *
 * @param pTree  pointer to the tree
 * @param pNode  pointer to the node we are getting the next one from
 *
 * @return  a pointer to the next node or NULL
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This is a thread safe implementation.
 */
rb_elem_t * rb_tree_get_next_from_node(rb_tree_t *pTree, rb_elem_t *pNode)
{
	rb_elem_t  *pElement = NULL;

	pthread_mutex_lock(&pTree->mutex);
	{
		pElement = __rb_tree_get_next_from_node(pTree, pNode);
	}
	pthread_mutex_unlock(&pTree->mutex);

	return (pElement);
}

/**
 * @brief    Walk the tree in no particular order
 * @details
 *
 * @param pTree      pointer to the tree we are walking
 * @param fActionCB  callback pointer called for each tree node
 * @param pData      transparent pointer to the data passed to the @p fActionCB callback
 * @param pOut
 *
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This function is not thread safe. Use the @ref rb_tree_walk()
 *        function if concurrency is an issue.
 */
void __rb_tree_walk(rb_tree_t *pTree, rb_walk_cb fActionCB, void *pData, void *pOut)
{
	if (pTree) {
		rb_walk(pTree, pTree->pRoot, fActionCB, pData, 0, pOut);
	}
}

/**
 * @brief    Walk the tree in no particular order
 * @details
 *
 * @param pTree      pointer to the tree we are walking
 * @param fActionCB  callback pointer called for each tree node
 * @param pData      transparent pointer to the data passed to the @p fActionCB callback
 * @param pOut
 *
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This is a thread safe implementation.
 */
void rb_tree_walk(rb_tree_t *pTree, rb_walk_cb fActionCB, void *pData, void *pOut)
{
	if (pTree) {
		pthread_mutex_lock(&pTree->mutex);
		{
			rb_walk(pTree, pTree->pRoot, fActionCB, pData, 0, pOut);
		}
		pthread_mutex_unlock(&pTree->mutex);
	}
}

/**
 * @brief    Walk the tree in order only.
 * @details  When we only need to walk the tree and have the callback done in
 *           sorted order, this is more efficient. Plus the callback does not
 *           need to check the order
 *
 * @param pTree      pointer to the tree we are walking
 * @param fActionCB  callback pointer called for each tree node
 * @param pData      transparent pointer to the data passed to the @p fActionCB callback
 * @param pOut
 *
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This function is not thread safe. Use the @ref rb_tree_walk_inorder()
 *        function if concurrency is an issue.
 */
void __rb_tree_walk_inorder(rb_tree_t *pTree, rb_walk_inorder_cb fActionCB, void *pData, void *pOut)
{
	if (pTree) {
		rb_walk_inorder(pTree, pTree->pRoot, fActionCB, pData, 0, pOut);
	}
}

/**
 * @brief    Walk the tree in order only.
 * @details  When we only need to walk the tree and have the callback done in
 *           sorted order, this is more efficient. Plus the callback does not
 *           need to check the order
 *
 * @param pTree      pointer to the tree we are walking
 * @param fActionCB  callback pointer called for each tree node
 * @param pData      transparent pointer to the data passed to the @p fActionCB callback
 * @param pOut
 *
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This is a thread safe implementation.
 */
void rb_tree_walk_inorder(rb_tree_t *pTree, rb_walk_inorder_cb fActionCB, void *pData, void *pOut)
{
	if (pTree) {
		pthread_mutex_lock(&pTree->mutex);
		{
			rb_walk_inorder(pTree, pTree->pRoot, fActionCB, pData, 0, pOut);
		}
		pthread_mutex_unlock(&pTree->mutex);
	}
}

/**
 * @brief    Step through the tree function
 * @details
 *
 * @param pTree     pointer to the tree
 * @param pElement  pointer to the current element
 * @param dir       direction. <0 = left or >0 = right
 *
 * @return  Pointer to the next element or NULL if no more.
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This function is not thread safe. Use the @ref rb_tree_step()
 *        function if concurrency is an issue.
 */
rb_elem_t * __rb_tree_step(rb_tree_t *pTree, rb_elem_t *pElement, int dir)
{
	rb_node_t  *pNode = NULL;

	if (pTree == NULL) {
		return (NULL);
	}

	if (!pElement) {
		pNode = pTree->pRoot;
	} else {
		pNode = RB_NODE(pTree, pElement);
		if (dir > 0) {
			pNode = pNode->pRight;
		} else if (dir < 0) {
			pNode = pNode->pLeft;
		}
	}

	return (((pNode == RB_ROOT(pTree)) ? NULL : RB_ELEMENT(pTree, pNode)));
}

/**
 * @brief    Step through the tree function
 * @details
 *
 * @param pTree     pointer to the tree
 * @param pElement  pointer to the current element
 * @param dir       direction. <0 = left or >0 = right
 *
 * @return  Pointer to the next element or NULL if no more.
 * @author  charles.gelinas@smartrg.com
 *
 * @note  This is a thread safe implementation.
 */
rb_elem_t * rb_tree_step(rb_tree_t *pTree, rb_elem_t *pElement, int dir)
{
	rb_elem_t  *pElem = NULL;

	if (pTree != NULL) {
		pthread_mutex_lock(&pTree->mutex);
		{
			pElem = __rb_tree_step(pTree, pElement, dir);
		}
		pthread_mutex_unlock(&pTree->mutex);
	}
	return (pElem);
}

/**
 * @brief    Search for and if not found and insert is true, will add a new node in.
 * @details
 *
 * @param insert
 * @param pElement
 * @param pTree
 * @param fCompare
 *
 * @return  a pointer to the new node, or the node found
 * @author  charles.gelinas@smartrg.com
 */
static rb_node_t * rb_traverse(bool insert, rb_elem_t *pElement, rb_tree_t *pTree, rb_compare_cb fCompare)
{
	bool        found  = false;
	int32_t     cmp    = 0;
	rb_node_t  *pNodeX = NULL;
	rb_node_t  *pNodeY = NULL;
	rb_node_t  *pNodeZ = NULL;

	if (pTree == NULL) {
		return (NULL);
	}

	fCompare = (fCompare == NULL) ? (pTree->fCompare) : fCompare;

	pNodeY = RB_ROOT(pTree); // points to the parent of pNodeX
	pNodeX = pTree->pRoot;

	// walk pNodeX down the pTree
	while ((pNodeX != RB_ROOT(pTree)) && !found) {
		pNodeY = pNodeX;

		cmp = (*fCompare)(pElement, RB_ELEMENT(pTree, pNodeX));
		if (cmp < 0) {
			pNodeX = pNodeX->pLeft;
		} else if (cmp > 0) {
			pNodeX = pNodeX->pRight;
		} else {
			found = true;
		}
	}

	if (found && insert) {
		return (RB_ROOT(pTree));
	}

	if (!insert) {
		return (pNodeX);
	}

	pNodeZ = RB_NODE(pTree, pElement);
	pNodeZ->pUp = pNodeY;

	if (pNodeY == RB_ROOT(pTree)) {
		pTree->pRoot = pNodeZ;
	} else {
		cmp = (*fCompare)(pElement, RB_ELEMENT(pTree, pNodeY));
		if (cmp < 0) {
			pNodeY->pLeft = pNodeZ;
		} else {
			pNodeY->pRight = pNodeZ;
		}
	}

	pNodeZ->pLeft  = RB_ROOT(pTree);
	pNodeZ->pRight = RB_ROOT(pTree);

	// color this new node red
	pNodeZ->color = RB_RED;

	//
	// Having added a red node, we must now walk back up the pTree balancing
	// it, by a series of rotations and changing of colors
	//
	pNodeX = pNodeZ;

	//
	// While we are not at the top and our parent node is red
	// N.B. Since the root node is garanteed black, then we
	// are also going to stop if we are the child of the root
	//
	while (pNodeX != pTree->pRoot && (pNodeX->pUp->color == RB_RED)) {
		// if our parent is on the pLeft side of our grandparent
		if (pNodeX->pUp == pNodeX->pUp->pUp->pLeft) {
			// get the pRight side of our grandparent (uncle?)
			pNodeY = pNodeX->pUp->pUp->pRight;
			if (pNodeY->color == RB_RED) {
				// make our parent black
				pNodeX->pUp->color = RB_BLACK;

				// make our uncle black
				pNodeY->color = RB_BLACK;

				// make our grandparent red
				pNodeX->pUp->pUp->color = RB_RED;

				// now consider our grandparent
				pNodeX = pNodeX->pUp->pUp;
			} else {
				// if we are on the pRight side of our parent
				if (pNodeX == pNodeX->pUp->pRight) {
					// Move up to our parent
					pNodeX = pNodeX->pUp;
					rb_rotate_left(&pTree->pRoot, pNodeX, pTree);
				}

				// make our parent black
				pNodeX->pUp->color = RB_BLACK;

				// make our grandparent red
				pNodeX->pUp->pUp->color = RB_RED;

				// pRight rotate our grandparent
				rb_rotate_right(&pTree->pRoot, pNodeX->pUp->pUp, pTree);
			}
		} else {
			// everything here is the same as above, but exchanging pLeft for pRight
			pNodeY = pNodeX->pUp->pUp->pLeft;
			if (pNodeY->color == RB_RED) {
				pNodeX->pUp->color = RB_BLACK;
				pNodeY->color = RB_BLACK;
				pNodeX->pUp->pUp->color = RB_RED;

				pNodeX = pNodeX->pUp->pUp;
			} else {
				if (pNodeX == pNodeX->pUp->pLeft) {
					pNodeX = pNodeX->pUp;
					rb_rotate_right(&pTree->pRoot, pNodeX, pTree);
				}

				pNodeX->pUp->color = RB_BLACK;
				pNodeX->pUp->pUp->color = RB_RED;
				rb_rotate_left(&pTree->pRoot, pNodeX->pUp->pUp, pTree);
			}
		}
	}

	// Set the root node black
	pTree->pRoot->color = RB_BLACK;
	return (pNodeZ);
}

/**
 * @brief    Destroy all the elements blow us in the tree only useful as part of a complete tree destroy.
 * @details
 *
 * @param pTree
 * @param pNode
 * @param fFreeCB
 * @param pData
 *
 * @author  charles.gelinas@smartrg.com
 */
static void rb_destroy(rb_tree_t *pTree, rb_node_t *pNode, rb_key_free_cb fFreeCB, void *pData)
{
	if (pTree) {
		if (pNode != RB_ROOT(pTree)) {
			if (pNode->pLeft != RB_ROOT(pTree)) {
				rb_destroy(pTree, pNode->pLeft, fFreeCB, pData);
			}

			if (pNode->pRight != RB_ROOT(pTree)) {
				rb_destroy(pTree, pNode->pRight, fFreeCB, pData);
			}

			if (fFreeCB) {
				fFreeCB(RB_ELEMENT(pTree, pNode), pData);
			}
		}
	}
}

//
/**
 * @brief    Delete the node z, and free up the space
 * @details
 *
 * @param ppRoot
 * @param pNode
 * @param pTree
 *
 * @author  charles.gelinas@smartrg.com
 */
static void rb_delete(rb_node_t **ppRoot, rb_node_t *pNode, rb_tree_t *pTree)
{
	rb_node_t        *pNodeX = NULL;
	rb_node_t        *pNodeY = NULL;
	rb_node_color_e   yColor = RB_BLACK;

	if (pNode->pLeft == RB_ROOT(pTree) || pNode->pRight == RB_ROOT(pTree)) {
		pNodeY = pNode;
	} else {
		pNodeY = rb_successor(pNode, pTree);
	}

	if (pNodeY->pLeft != RB_ROOT(pTree)) {
		pNodeX = pNodeY->pLeft;
	} else {
		pNodeX = pNodeY->pRight;
	}

	pNodeX->pUp = pNodeY->pUp;
	if (pNodeY->pUp == RB_ROOT(pTree)) {
		(*ppRoot) = pNodeX;
	} else {
		if (pNodeY == pNodeY->pUp->pLeft) {
			pNodeY->pUp->pLeft = pNodeX;
		} else {
			pNodeY->pUp->pRight = pNodeX;
		}
	}

	yColor = pNodeY->color;

	if (pNodeY != pNode) {
		// if root node
		if (pNode->pUp == RB_ROOT(pTree)) {
			(*ppRoot) = pNodeY;
		}

		// make pNodeY point to parent of pNode
		pNodeY->pUp = pNode->pUp;

		// make parent of pNode pt to pNodeY
		if (pNode->pUp->pLeft == pNode) {
			pNode->pUp->pLeft = pNodeY;
		} else {
			pNode->pUp->pRight = pNodeY;
		}

		// make the chldrn of pNode pt to pNodeY
		pNode->pLeft->pUp = pNodeY;
		pNode->pRight->pUp = pNodeY;

		// make pNodeY point to chldrn of pNode
		pNodeY->pLeft = pNode->pLeft;
		pNodeY->pRight = pNode->pRight;

		pNodeY->color = pNode->color;
	}

	if (yColor == RB_BLACK) {
		rb_delete_fix(ppRoot, pNodeX, pTree);
	}
}

/**
 * @brief    Restore the reb-black properties after a delete
 * @details
 *
 * @param ppRoot
 * @param pNode
 * @param pTree
 *
 * @author  charles.gelinas@smartrg.com
 */
static void rb_delete_fix(rb_node_t **ppRoot, rb_node_t *pNode, rb_tree_t *pTree)
{
	rb_node_t  *pNodeX = NULL;

	while ((pNode != (*ppRoot)) && (pNode->color == RB_BLACK)) {
		if (pNode == pNode->pUp->pLeft) {
			pNodeX = pNode->pUp->pRight;

			if (pNodeX->color == RB_RED) {
				pNodeX->color = RB_BLACK;
				pNode->pUp->color = RB_RED;
				rb_rotate_left (ppRoot, pNode->pUp, pTree);
				pNodeX = pNode->pUp->pRight;
			}

			if ((pNodeX->pLeft->color == RB_BLACK) && (pNodeX->pRight->color == RB_BLACK)) {
				pNodeX->color = RB_RED;
				pNode = pNode->pUp;
			} else {
				if (pNodeX->pRight->color == RB_BLACK) {
					pNodeX->pLeft->color = RB_BLACK;
					pNodeX->color = RB_RED;
					rb_rotate_right(ppRoot, pNodeX, pTree);
					pNodeX = pNode->pUp->pRight;
				}

				pNodeX->color = pNode->pUp->color;
				pNode->pUp->color = RB_BLACK;
				pNodeX->pRight->color = RB_BLACK;
				rb_rotate_left(ppRoot, pNode->pUp, pTree);
				pNode = (*ppRoot);
			}
		} else {
			pNodeX = pNode->pUp->pLeft;

			if (pNodeX->color == RB_RED) {
				pNodeX->color = RB_BLACK;
				pNode->pUp->color = RB_RED;
				rb_rotate_right (ppRoot, pNode->pUp, pTree);
				pNodeX = pNode->pUp->pLeft;
			}

			if ((pNodeX->pRight->color == RB_BLACK) && (pNodeX->pLeft->color == RB_BLACK)) {
				pNodeX->color = RB_RED;
				pNode = pNode->pUp;
			} else {
				if (pNodeX->pLeft->color == RB_BLACK) {
					pNodeX->pRight->color = RB_BLACK;
					pNodeX->color = RB_RED;
					rb_rotate_left(ppRoot, pNodeX, pTree);
					pNodeX = pNode->pUp->pLeft;
				}

				pNodeX->color = pNode->pUp->color;
				pNode->pUp->color = RB_BLACK;
				pNodeX->pLeft->color = RB_BLACK;
				rb_rotate_right(ppRoot, pNode->pUp, pTree);
				pNode = (*ppRoot);
			}
		}
	}

	pNode->color = RB_BLACK;
}

/**
 * @brief
 * @details
 *
 * @param pTree
 * @param pNode
 * @param fAction
 * @param pData
 * @param level
 * @param pOut
 *
 * @return  @ref RB_WALK_BREAK or RB_WALK_CONT
 * @author  charles.gelinas@smartrg.com
 */
static rb_walk_returns_e rb_walk(rb_tree_t *pTree, rb_node_t *pNode, rb_walk_cb fAction, void *pData, uint32_t level, void *pOut)
{
	rb_elem_t          *pElem = RB_ELEMENT(pTree, pNode);
	rb_walk_returns_e   rc    = RB_WALK_BREAK;

	if (pNode == RB_ROOT(pTree)) {
		return (RB_WALK_CONT);
	}

	if ((pNode->pLeft == RB_ROOT(pTree)) && (pNode->pRight == RB_ROOT(pTree))) {
		// leaf
		rc = (*fAction)(pElem, RB_VISIT_LEAF, level, pData, pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}
	} else {
		rc = (*fAction)(pElem, RB_VISIT_PREORDER, level, pData, pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}

		rc = rb_walk(pTree, pNode->pLeft, fAction, pData, (level + 1), pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}

		rc = (*fAction)(pElem, RB_VISIT_POSTORDER, level, pData, pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}

		rc = rb_walk(pTree, pNode->pRight, fAction, pData, (level + 1), pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}

		rc = (*fAction)(pElem, RB_VISIT_ENDORDER, level, pData, pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}
	}

	return (RB_WALK_CONT);
}

/**
 * @brief
 * @details
 *
 * @param pTree
 * @param pNode
 * @param fAction
 * @param pData
 * @param level
 * @param pOut
 *
 * @return  @ref RB_WALK_BREAK or RB_WALK_CONT
 * @author  charles.gelinas@smartrg.com
 */
static rb_walk_returns_e rb_walk_inorder(rb_tree_t *pTree,
											rb_node_t *pNode,
											rb_walk_inorder_cb fAction,
											void *pData,
											uint32_t level,
											void *pOut)
{
	rb_elem_t          *pElem = RB_ELEMENT(pTree, pNode);
	rb_walk_returns_e   rc    = RB_WALK_BREAK;

	if (pNode == RB_ROOT(pTree)) {
		return (RB_WALK_CONT);
	}

	if ((pNode->pLeft == RB_ROOT(pTree)) && (pNode->pRight == RB_ROOT(pTree))) {
		// leaf
		rc = (*fAction)(pElem, level, pData, pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}
	} else {
		//
		// Walk pLeft to process that which is lower order than us.
		// then call the callback for ourself then walk pRight to
		// process that which is higher order than us.
		//
		rc = rb_walk_inorder(pTree, pNode->pLeft, fAction, pData, (level + 1), pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}

		rc = (*fAction)(pElem, level, pData, pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}

		rc = rb_walk_inorder(pTree, pNode->pRight, fAction, pData, (level + 1), pOut);
		if (rc == RB_WALK_BREAK) {
			return (RB_WALK_BREAK);
		}
	}

	return (RB_WALK_CONT);
}

/**
 * @brief    Rotate our pTree
 * @details
 *             X        rb_rotate_left(X)--->            Y
 *           /   \                                     /   \
 *          A     Y     <---rb_rotate_right(Y)        X     C
 *              /   \                               /   \
 *             B     C                             A     B
 *
 * N.B. This does not change the ordering.
 *
 * We assume that neither X or Y is NULL
 *
 * @param ppRoot
 * @param pNode
 * @param pTree
 *
 * @author  charles.gelinas@smartrg.com
 */
static void rb_rotate_left(rb_node_t **ppRoot, rb_node_t *pNode, rb_tree_t *pTree)
{
	rb_node_t  *pNodeY = NULL;

	//
	// Preconditions:
	//  (pNode != RB_ROOT(pTree));
	//  (pNode->pRight != RB_ROOT(pTree));
	//
	pNodeY = pNode->pRight;

	// Turn Y's pLeft subtree into X's pRight subtree (move B)
	pNode->pRight = pNodeY->pLeft;

	// If B is not null, set it's parent to be X
	if (pNodeY->pLeft != RB_ROOT(pTree)) {
		pNodeY->pLeft->pUp = pNode;
	}

	// Set Y's parent to be what X's parent was
	pNodeY->pUp = pNode->pUp;

	// if X was the root
	if (pNode->pUp == RB_ROOT(pTree)) {
		(*ppRoot) = pNodeY;
	} else {
		// Set X's parent's pLeft or pRight pointer to be Y
		if (pNode == pNode->pUp->pLeft) {
			pNode->pUp->pLeft = pNodeY;
		} else {
			pNode->pUp->pRight = pNodeY;
		}
	}

	// Put X on Y's pLeft
	pNodeY->pLeft = pNode;

	// Set X's parent to be Y
	pNode->pUp = pNodeY;
}

/**
 * @brief
 * @details
 *
 * @param ppRoot
 * @param pNode
 * @param pTree
 *
 * @author  charles.gelinas@smartrg.com
 */
static void rb_rotate_right(rb_node_t **ppRoot, rb_node_t *pNode, rb_tree_t *pTree)
{
	rb_node_t  *pNodeY = NULL;

	//
	// Preconditions
	//  (pNode != RB_ROOT(pTree));
	//  (pNode->pLeft != RB_ROOT(pTree));
	//
	pNodeY = pNode->pLeft;

	// Turn Y's pRight subtree into pNode's pLeft subtree (move B)
	pNode->pLeft = pNodeY->pRight;

	// If B is not null, set it's parent to be Y
	if (pNodeY->pRight != RB_ROOT(pTree)) {
		pNodeY->pRight->pUp = pNode;
	}

	// Set Y's parent to be what pNode's parent was
	pNodeY->pUp = pNode->pUp;

	// if pNode was the root
	if (pNode->pUp == RB_ROOT(pTree)) {
		(*ppRoot) = pNodeY;
	} else {
		// Set pNode's parent's pLeft or pRight pointer to be Y
		if (pNode == pNode->pUp->pLeft) {
			pNode->pUp->pLeft = pNodeY;
		} else {
			pNode->pUp->pRight = pNodeY;
		}
	}

	// Put Y on X's pRight
	pNodeY->pRight = pNode;

	// Set Y's parent to be X
	pNode->pUp = pNodeY;
}

/**
 * @brief    Return a pointer to the smallest key greater than pNode
 * @details
 *
 * @param pNode
 * @param pTree
 *
 * @return
 * @author  charles.gelinas@smartrg.com
 */
static rb_node_t * rb_successor(rb_node_t *pNode, rb_tree_t *pTree)
{
	rb_node_t  *pNodeY = NULL;

	if (pNode->pRight != RB_ROOT(pTree)) {
		//
		// If pRight is not NULL then go pRight one and
		// then keep going pLeft until we find a node with
		// no pLeft pointer.
		//
		for (pNodeY = pNode->pRight; pNodeY->pLeft != RB_ROOT(pTree); pNodeY = pNodeY->pLeft)
			;
	} else {
		//
		// Go up the pTree until we get to a node that is on the
		// pLeft of its parent (or the root) and then return the
		// parent.
		//
		pNodeY = pNode->pUp;

		while ((pNodeY != RB_ROOT(pTree)) && (pNode == pNodeY->pRight)) {
			pNode  = pNodeY;
			pNodeY = pNodeY->pUp;
		}
	}

	return (pNodeY);
}
