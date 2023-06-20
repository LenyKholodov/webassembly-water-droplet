#pragma once

#include <list>
#include <memory>

#include <common/uninitialized_storage.h>

template <class T>
class Pool
{
  public:
    enum { DEFAULT_PAGE_SIZE = 100 };

    /// Constructor
    Pool(size_t pageSize = DEFAULT_PAGE_SIZE);

    /// Destructor
    ~Pool();

    /// Set blocks count for the page
    void set_page_size(size_t inPageSize);

    /// Get blocks count for the page
    size_t page_size() const;

    /// Reserve requested blocks count
    void reserve(size_t blocksCount);

    /// Capacity
    size_t capacity() const { return capacity_; }

    /// Capacity in bytes
    size_t capacity_in_bytes() const { return capacity_ * sizeof(Block); }

    /// Allocate block
    T* allocate();

    /// Deallocate block
    void deallocate(T* block);

    /// Reset pool(deallocate all blocks)
    void reset();

  private:
    Pool(const Pool&); //no implementation
    Pool& operator =(const Pool&); //no implementation

    struct Page;

    T* allocate(Page& page);
    void reset(Page& page);

  private:
    struct Block
    {
      T data;

      union
      {
        Block* next;
        Page*  page;
      };
    };

    struct Page
    {
      engine::common::UninitializedStorage<Block> blocks;
      size_t blocks_count;
      Block* first;
    };

    typedef std::shared_ptr<Page> PagePtr;
    typedef std::list<PagePtr>    PageList;

  private:
    PageList pages_;
    size_t   page_size_;
    size_t   free_blocks_count_;
    size_t   capacity_;
};

template <class T>
Pool<T>::Pool(size_t in_page_size_)
  : page_size_(in_page_size_)
  , free_blocks_count_()
  , capacity_()
{
}

template <class T>
Pool<T>::~Pool()
{
}

template <class T>
void Pool<T>::set_page_size(size_t in_page_size)
{
  page_size_ = in_page_size;
}

template <class T>
size_t Pool<T>::page_size() const
{
  return page_size_;
}

template <class T>
void Pool<T>::reserve(size_t blocks_count)
{
  if(!blocks_count)
    return;

  if(blocks_count <= free_blocks_count_)
    return;

  blocks_count -= free_blocks_count_;

  if(blocks_count < page_size_)
    blocks_count = page_size_;

  PagePtr page(new Page);

  page->blocks.resize(blocks_count);

  page->blocks_count = blocks_count;

  reset(*page);

  pages_.push_back(page);

  capacity_ += blocks_count;
}

template <class T>
void Pool<T>::reset(Page& page)
{
  size_t blocks_count = page.blocks_count;
  Block* block = page.blocks.data();

  page.first = block;

  for(size_t i=0; i<blocks_count; i++, block++)
  {
    block->next = block + 1;
  }

  block[-1].next = 0;

  free_blocks_count_ += blocks_count;
}

template <class T>
void Pool<T>::reset()
{
  free_blocks_count_ = 0;

  for(typename PageList::iterator iter=pages_.begin(), end=pages_.end(); iter!=end; ++iter)
  {
    Page& page = **iter;
    reset(page);
  }
}

template <class T>
T* Pool<T>::allocate()
{
  for(typename PageList::iterator iter=pages_.begin(), end=pages_.end(); iter!=end; ++iter)
  {
    Page& page = **iter;

    if(page.first)
      return allocate(page);
  }

  reserve(page_size_);

  return allocate(*pages_.back());
}

template <class T>
T* Pool<T>::allocate(Page& page)
{
  Block* block = page.first;

  if(!block)
    return 0;

  page.first = block->next;

  block->page = &page;

  free_blocks_count_--;

  return &block->data;
}

template <class T>
void Pool<T>::deallocate(T* data)
{
  if(!data)
    return;

  Block* block = reinterpret_cast<Block*>(data);
  Page&  page  = *block->page;

  block->next = page.first;
  page.first  = block;

  free_blocks_count_++;
}
