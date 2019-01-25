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
 * @file      sos_hlist.h
 * @authors   charles.gelinas@smartrg.com
 *
 * @brief     Defines a managed hash list
 * @details
 *
 * @version   1.0
 *
 * @copyright 2017 by SmartRG, Inc.
 *******************************************************************************/

#ifndef _SOS_HLIST_H_
#define _SOS_HLIST_H_

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *                                                                             *
 *                                    INCLUDES                                 *
 *                                                                             *
 *******************************************************************************/

/*******************************************************************************
 *                                                                             *
 *                                    DEFINES                                  *
 *                                                                             *
 *******************************************************************************/

//! Enable this flag to enable debugging
//#define SOS_HLIST_DEBUG

#define SOS_HLIST_BITS							(10)					//!< Number of bits used to define the hash list size
#define SOS_HLIST_SIZE							(1 << SOS_HLIST_BITS)	//!< Hash list size in power of 2 for efficiency

//!< Poison for the @ref sos_hhead::pNext node pointer
#define SOS_HLIST_POISON1						((sos_hhead_t *)((uintptr_t) 0xDEAD0100))

//!< Poison for the @ref sos_hhead::pPrev node pointer
#define SOS_HLIST_POISON2						((sos_hhead_t *)((uintptr_t) 0xDEAD0200))

//!< Poison for the @ref sos_hhead::pHash pointer
#define SOS_HLIST_POISON3						((sos_hash_t *)((uintptr_t) 0xDEAD0300))

/*******************************************************************************
 *                                                                             *
 *                                   TYPEDEFS                                  *
 *                                                                             *
 *******************************************************************************/

//! Node into a hashed list
typedef struct sos_hhead {
	struct sos_hhead		*pNext;		//!< pointer to the next element in the list
	struct sos_hhead		*pPrev;		//!< pointer to the previous element in the list
	struct sos_hash			*pHash;		//!< pointer to our associated hash
} sos_hhead_t;

//! Hash structure
typedef struct sos_hash {
	struct sos_hhead		 head;		//!< head of the list
#ifdef SOS_HLIST_DEBUG
	struct sos_hlist		*pList;		//!< Pointer to our associated hash list
	uint32_t				 hash;		//!< The hash identifier for this hash
	uint32_t				 items;		//!< Number of elements currently associated with this hash
	uint32_t				 max;		//!< Maximum number of elements seen at one time
#endif
} sos_hash_t;

//! Hash List implementation
typedef struct sos_hlist {
#ifdef SOS_HLIST_DEBUG
	uint32_t				 items;					//!< Number of elements currently associated with this hash list
	uint32_t				 max;					//!< Maximum number of elements seen at one time
#endif
	struct sos_hash			 aHash[SOS_HLIST_SIZE];	//!< Array of hash
} sos_hlist_t;

/*******************************************************************************
 *                                                                             *
 *                              GLOBAL PROTOTYPES                              *
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

//! @{
//! @name MIN/MAX Fast Macros
//!
#define SOS_HLIST_MIN(_a, _b)						((_b) + (((_a) - (_b)) & -((_a) < (_b))))
#define SOS_HLIST_MAX(_a, _b)						((_a) - (((_a) - (_b)) & -((_a) < (_b))))
//!
//! @}

//! Macro to compute a hash and ensure the type of the key
#define __sos_hlist_hash(_pKey, _len, _bits)		sos_hlist_hash(((const uint8_t *)(_pKey)), _len, _bits)

//! Convert a node to a given type
#define sos_hlist_entry(_ptr, _type, _member)		container_of((_ptr), _type, _member)

/**
 * @brief  list_first_entry - get the first element from a list
 *
 * @param _ptr     pointer to the list head to take the element from.
 * @param _type    the type of the struct this is embedded in.
 * @param _member  the name of the hash node member within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define sos_hlist_first_entry(_ptr, _type, _member)	sos_hlist_entry((_ptr)->pNext, _type, _member)

/**
 * @brief  list_next_entry - get the next element in list
 *
 * @param _pos     the type * to cursor
 * @param _member  the name of the list_struct within the struct.
 */
#define sos_hlist_next_entry(_pos, _member)			sos_hlist_entry((_pos)->_member.pNext, typeof(*_pos), _member)

/**
 * @brief  list_for_each_entry - iterate over list of given type
 *
 * @param _pos     the type * to use as a loop cursor.
 * @param _head    the head for your list.
 * @param _member  the name of the list_struct within the struct.
 */
#define sos_hlist_for_each_entry(_pos, _head, _member)						\
	for ((_pos) = sos_hlist_first_entry((_head), typeof(*_pos), _member);	\
			&(_pos)->_member != (_head);									\
				(_pos) = sos_hlist_next_entry((_pos), _member))

/**
 * @brief  list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 *
 * @param _pos     the type * to use as a loop cursor.
 * @param _next    another type * to use as temporary storage
 * @param _head    the head for your list.
 * @param _member  the name of the list_struct within the struct.
 */
#define sos_hlist_for_each_entry_safe(_pos, _next, _head, _member)															\
	for ((_pos) = sos_hlist_first_entry((_head), typeof(*_pos), _member), (_next) = sos_hlist_next_entry((_pos), _member);	\
			&(_pos)->_member != (_head); 																					\
				(_pos) = (_next), (_next) = sos_hlist_next_entry((_next), _member))

/*******************************************************************************
 *                                                                             *
 *                                   INLINES                                   *
 *                                                                             *
 *******************************************************************************/

/**
 * @brief  Computes a 32 bits one at the time hash of a given string
 *
 * @param pKey  pointer to the data/key to hash
 * @param len   the length of the key to hash
 * @param bits  the number of bits to use for the hash value
 *
 * @return  a 32 bits hash value
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ uint32_t sos_hlist_hash(const uint8_t *pKey, const size_t len, const uint8_t bits)
{
	register uint32_t  i    = 0;
	register uint32_t  hash = 0;

	for (hash = 0, i = 0; i < len; i++) {
		hash += pKey[i];
		hash += (hash << 10);
		hash ^= (hash >>  6);
	}

	hash += (hash <<  3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return ((uint32_t) (hash >> (32 - bits)));
}

/**
 * @brief  Initialize a hash node
 *
 * @param pNode  pointer to the node to initialize
 *
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ void __sos_hlist_node_init(sos_hhead_t *pNode)
{
	pNode->pNext = pNode;
	pNode->pPrev = pNode;
	pNode->pHash = NULL;
}

/**
 * @brief  Initialize a given hash list
 *
 * @param pList  pointer to the hash list to initialize
 *
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ void __sos_hlist_init(sos_hlist_t *pList)
{
	uint32_t              hash  = 0;
	register size_t       i     = 0;
	register sos_hash_t  *pHash = NULL;

	// Zero out our hasharray
	memset(pList->aHash, 0, sizeof(pList->aHash));

	// Initialize all of the hashes individually
	for (i = SOS_HLIST_SIZE, pHash = pList->aHash, hash = 0; i > 0; i--, pHash++, hash++) {
		// Initialize our list head
		__sos_hlist_node_init(&pHash->head);

#ifdef SOS_HLIST_DEBUG
		// Remember our hash identifier
		pHash->hash = hash;

		// Remember our list
		pHash->pList = pList;
#endif
	}
}

#ifdef SOS_HLIST_DEBUG
/**
 * @brief  Reset the hash list max counters
 *
 * @param pList  pointer to the hash list to reset
 *
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ void __sos_hlist_reset_max(sos_hlist_t *pList)
{
	register size_t       i     = 0;
	register sos_hash_t  *pHash = NULL;

	// Reset our hash list max
	pList->max = pList->items;

	// Reset the max of all of our individual hash
	for (i = SOS_HLIST_SIZE, pHash = pList->aHash; i > 0; i--, pHash++) {
		pHash->max = pHash->items;
	}
}
#endif

/**
 * @brief  Check if a give node is part of a hashed list
 *
 * @param pNode  pointer to the node to validate
 *
 * @return  true if the node is not part of a hashed list otherwise false is returned.
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ bool sos_hlist_unhashed(const sos_hhead_t *pNode)
{
	return ((pNode->pHash == NULL) || (pNode->pHash == SOS_HLIST_POISON3));
}

/**
 * @brief  Checks whether or not a given hash list head is empty
 *
 * @param pHash  pointer to the hash to validate
 *
 * @return  true if the list is empty otherwise false is returned
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ bool sos_hlist_empty(const sos_hash_t *pHash)
{
	register const sos_hhead_t  *pHead = &pHash->head;
	return (pHead->pNext == pHead);
}

/**
 * @brief  Adds a hash node between two other nodes
 *
 * @param pNew   pointer to the new node to add
 * @param pPrev  pointer to our previous node in the list
 * @param pNext  pointer to our next node in the list
 *
 * @author  charles.gelinas@smartrg.com
 */
static inline void __sos_hlist_add_node(sos_hhead_t *pNew, sos_hhead_t *pPrev, sos_hhead_t *pNext)
{
	pNext->pPrev = pNew;
	pNew->pNext  = pNext;
	pNew->pPrev  = pPrev;
	pPrev->pNext = pNew;
}

/**
 * @brief  Accounts for items added to a hash list under a given hash
 *
 * @param pHash  pointer to the hash to do the accounting
 *
 * @author  charles.gelinas@smartrg.com
 */
#ifndef SOS_HLIST_DEBUG
#define __sos_hlist_add_account(_pHash)			(void)(_pHash)
#else /* ! SOS_HLIST_DEBUG */
static __inline__ void __sos_hlist_add_account(sos_hash_t *pHash)
{
	sos_hlist_t  *pList = pHash->pList;

	// Compute the hash items counters
	pHash->items++;
	pHash->max = SOS_HLIST_MAX(pHash->max, pHash->items);

	// Compute the list items counters
	pList->items++;
	pList->max = SOS_HLIST_MAX(pList->max, pList->items);
}
#endif /* ! SOS_HLIST_DEBUG */

/**
 * @brief  Adds an item at the head of a hash from a hash list
 *
 * @param pNode  pointer to the node to add to the hash
 * @param pHash  pointer to the hash to add to
 *
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ void __sos_hlist_add(sos_hhead_t *pNode, sos_hash_t *pHash)
{
	register sos_hhead_t  *pHead = &pHash->head;

	// Insert the node
	__sos_hlist_add_node(pNode, pHead, pHead->pNext);

	// Set our hash pointer
	pNode->pHash = pHash;

	// Compute the hash items counters
	__sos_hlist_add_account(pHash);
}

/**
 * @brief  Adds an item at the tail of a hash from a hash list
 *
 * @param pNode  pointer to the node to add to the hash
 * @param pHash  pointer to the hash to add to
 *
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ void __sos_hlist_add_tail(sos_hhead_t *pNode, sos_hash_t *pHash)
{
	register sos_hhead_t  *pHead = &pHash->head;

	// Insert the node
	__sos_hlist_add_node(pNode, pHead->pPrev, pHead);

	// Set our hash pointer
	pNode->pHash = pHash;

	// Compute the hash items counters
	__sos_hlist_add_account(pHash);
}

/**
 * @brief  Removes an item between 2 node in a hash list
 *
 * @param pNode  pointer to the node to remove from the hash
 *
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ void __sos_hlist_del_node(sos_hhead_t *pPrev, sos_hhead_t *pNext)
{
	pNext->pPrev = pPrev;
	pPrev->pNext = pNext;
}

/**
 * @brief  Accounts for items removed from a hash list under a given hash
 *
 * @param pHash  pointer to the hash to do the accounting
 *
 * @author  charles.gelinas@smartrg.com
 */
#ifndef SOS_HLIST_DEBUG
#define __sos_hlist_del_account(_pHash)			(void)(_pHash)
#else /* ! SOS_HLIST_DEBUG */
static __inline__ void __sos_hlist_del_account(sos_hash_t *pHash)
{
	register sos_hlist_t  *pList = pHash->pList;

	// Do our hash and list accounting
	pHash->items--;
	pList->items--;
}
#endif /* ! SOS_HLIST_DEBUG */

/**
 * @brief  Removes an item in a hash list and re-init the node
 *
 * @param pNode  pointer to the node to remove from the hash
 *
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ void __sos_hlist_del_reinit(sos_hhead_t *pNode)
{
	register sos_hash_t  *pHash = NULL;

	// Make sure this node is in a hash list
	if (!sos_hlist_unhashed(pNode)) {
		// Get our hash
		pHash = pNode->pHash;

		// Remove the node from the hash list
		__sos_hlist_del_node(pNode->pPrev, pNode->pNext);

		// Re-initialize the node
		__sos_hlist_node_init(pNode);

		// Do our hash and list accounting
		__sos_hlist_del_account(pHash);
	}
}

/**
 * @brief  Removes an item in a hash list and poison the entry.
 *
 * @param pNode  pointer to the node to remove from the hash
 *
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ void __sos_hlist_del(sos_hhead_t *pNode)
{
	register sos_hash_t  *pHash = NULL;

	// Make sure this node is in a hash list
	if (!sos_hlist_unhashed(pNode)) {
		// Get our hash
		pHash = pNode->pHash;

		// Remove the node from the hash list
		__sos_hlist_del_node(pNode->pPrev, pNode->pNext);

		// Poison the node
		pNode->pNext = SOS_HLIST_POISON1;
		pNode->pPrev = SOS_HLIST_POISON2;
		pNode->pHash = SOS_HLIST_POISON3;

		// Do our hash and list accounting
		__sos_hlist_del_account(pHash);
	}
}

/**
 * @brief  Retrieve a hash from a hash list based on the given key information
 *
 * @param pList  pointer to the hash list to get the hash from
 * @param pKey   pointer to the data/key to hash
 * @param len    the length of the key to hash
 * @param bits   the number of bits to use for the hash value
 *
 * @return  a pointer to the hash
 * @author  charles.gelinas@smartrg.com
 */
static __inline__ sos_hash_t * __sos_hlist_get_hash(sos_hlist_t *pList, const uint8_t *pKey, const size_t len, const uint8_t bits)
{
	return (&pList->aHash[sos_hlist_hash(pKey, len, bits)]);
}

#ifdef __cplusplus
}
#endif
#endif /* _SOS_HLIST_H_ */
