#include "dib/types.h"

#include <unordered_map>
#include <typeindex>
#include <iostream>

using namespace dib::types;
using namespace dib;

bool TypeDescriptor::can_relocate() const { return _vt->_relocate; }
bool TypeDescriptor::can_move() const { return _vt->_move_construct; }
bool TypeDescriptor::can_copy() const { return _vt->_copy_construct; }
bool TypeDescriptor::can_default() const { return _vt->_copy_construct; }
bool TypeDescriptor::can_swap() const { return _vt->_swap; }

void TypeDescriptor::destruct(void *dest) const { _vt->_destruct(dest); }

void TypeDescriptor::uninitialized_move_construct(void *src, void *dest) const { _vt->_move_construct(src, dest); }
void TypeDescriptor::uninitialized_copy_construct(void *src, void *dest) const { _vt->_copy_construct(src, dest); }
void TypeDescriptor::uninitialized_relocate(void *src, void *dest) const { _vt->_relocate(src, dest); }
void TypeDescriptor::uninitialized_default_construct(void *src) const { _vt->_default_construct(src); }

void TypeDescriptor::move_construct(void *src, void *dest) const { destruct(dest); uninitialized_move_construct(src, dest); }
void TypeDescriptor::copy_construct(void *src, void *dest) const { destruct(dest); uninitialized_copy_construct(src, dest); }
void TypeDescriptor::relocate(void *src, void *dest) const { destruct(dest); uninitialized_relocate(src, dest); }
void TypeDescriptor::default_construct(void *src) const { destruct(src); uninitialized_default_construct(src); }

void TypeDescriptor::swap(void *src, void *dest) const { _vt->_swap(src, dest); }