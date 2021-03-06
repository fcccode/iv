#ifndef IV_LV5_JSARRAY_H_
#define IV_LV5_JSARRAY_H_
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <functional>
#include <iv/conversions.h>
#include <iv/lv5/error_check.h>
#include <iv/lv5/gc_template.h>
#include <iv/lv5/property.h>
#include <iv/lv5/jsval.h>
#include <iv/lv5/jsobject_fwd.h>
#include <iv/lv5/map.h>
#include <iv/lv5/slot.h>
#include <iv/lv5/class.h>
#include <iv/lv5/context.h>
#include <iv/lv5/storage.h>
#include <iv/lv5/railgun/fwd.h>
#include <iv/lv5/radio/core.h>
namespace iv {
namespace lv5 {

class Context;
class JSVector;

class JSArray : public JSObject {
 public:
  IV_LV5_DEFINE_JSCLASS(JSArray, Array)
  friend class railgun::VM;
  friend class JSVector;

  uint32_t length() const { return elements_.length(); }

  static JSArray* New(Context* ctx, uint32_t n = 0) {
    return new JSArray(ctx, n);
  }

  static JSArray* NewPlain(Context* ctx, Map* map) {
    return new JSArray(ctx, map, 0u);
  }

  static JSArray* New(Context* ctx, JSArray* array) {
    return new JSArray(ctx, *array);
  }

  IV_LV5_INTERNAL_METHOD bool GetOwnNonIndexedPropertySlotMethod(const JSObject* obj, Context* ctx, Symbol name, Slot* slot) {
    if (name == symbol::length()) {
      const JSArray* array = static_cast<const JSArray*>(obj);
      slot->set(
          JSVal::UInt32(array->elements_.length()),
          (array->elements_.writable()) ? ATTR::CreateData(ATTR::W) : ATTR::CreateData(ATTR::N),
          array);
      return true;
    }
    return JSObject::GetOwnNonIndexedPropertySlotMethod(obj, ctx, name, slot);
  }

  IV_LV5_INTERNAL_METHOD bool DefineOwnNonIndexedPropertySlotMethod(JSObject* obj,
                                                                    Context* ctx,
                                                                    Symbol name,
                                                                    const PropertyDescriptor& desc,
                                                                    Slot* slot,
                                                                    bool throwable,
                                                                    Error* e) {
    if (name == symbol::length()) {
      // section 15.4.5.1 step 3
      return static_cast<JSArray*>(obj)->DefineLengthProperty(ctx, desc, throwable, e);
    }
    // section 15.4.5.1 step 5
    return JSObject::DefineOwnNonIndexedPropertySlotMethod(obj, ctx, name, desc, slot, throwable, e);
  }

  IV_LV5_INTERNAL_METHOD bool DeleteNonIndexedMethod(JSObject* obj, Context* ctx, Symbol name, bool throwable, Error* e) {
    if (symbol::length() == name) {
      if (throwable) {
        e->Report(Error::Type, "delete failed");
      }
      return false;
    }
    return JSObject::DeleteNonIndexedMethod(obj, ctx, name, throwable, e);
  }

  IV_LV5_INTERNAL_METHOD void GetOwnPropertyNamesMethod(const JSObject* obj,
                                                        Context* ctx,
                                                        PropertyNamesCollector* collector,
                                                        EnumerationMode mode) {
    if (mode == INCLUDE_NOT_ENUMERABLE) {
      collector->Add(symbol::length(), 0);
    }
    JSObject::GetOwnPropertyNamesMethod(obj, ctx, collector, mode);
  }

  static JSArray* ReservedNew(Context* ctx, uint32_t length) {
    JSArray* array = new JSArray(ctx, length);
    if (length > IndexedElements::kMaxVectorSize) {
      array->elements_.vector.resize(IndexedElements::kMaxVectorSize, JSEmpty);
      array->elements_.EnsureMap();
    } else {
      array->elements_.vector.resize(length, JSEmpty);
    }
    return array;
  }

  template<typename Iter>
  void SetToVector(uint32_t index, Iter it, Iter last) {
    assert(IndexedElements::kMaxVectorSize > index);
    assert(elements_.vector.size() >=
           (index + static_cast<std::size_t>(std::distance(it, last))));
    std::copy(it, last, elements_.vector.begin() + index);
  }

  template<typename Iter>
  void SetToMap(uint32_t index, Iter it, Iter last) {
    assert(IndexedElements::kMaxVectorSize <= index);
    assert(elements_.map);
    for (; it != last; ++it, ++index) {
      if (!it->IsEmpty()) {
        elements_.map->insert(std::make_pair(index, StoredSlot(*it, ATTR::Object::Data())));
      }
    }
  }

 private:
  JSArray(Context* ctx, uint32_t len)
    : JSObject(ctx->global_data()->array_map()) {
    elements_.set_length(len);
//    elements_.vector.resize(
//        (std::min<uint32_t>)(
//            len,
//            static_cast<uint32_t>(IndexedElements::kMaxVectorSize)), JSEmpty);
    set_cls(GetClass());
  }

  JSArray(Context* ctx, Map* map, uint32_t len)
    : JSObject(map) {
    elements_.set_length(len);
//    elements_.vector.resize(
//        (std::min<uint32_t>)(
//            len,
//            static_cast<uint32_t>(IndexedElements::kMaxVectorSize)), JSEmpty);
    set_cls(GetClass());
  }

  JSArray(Context* ctx, const JSArray& array) : JSObject(array) { }

  // section 15.4.5.1 step 3
  bool DefineLengthProperty(Context* ctx,
                            const PropertyDescriptor& desc,
                            bool throwable, Error* e);

  bool ChangeLengthWritable(bool writable, bool throwable, Error* e);
  bool SetLength(Context* ctx, uint32_t len, bool throwable, Error* e);
};

inline bool JSArray::ChangeLengthWritable(bool writable, bool throwable, Error* e) {
  if (!writable) {
    elements_.MakeReadOnly();
  } else {
    // [[Writable]]: false
    if (!elements_.writable()) {
      if (throwable) {
        e->Report(Error::Type, "changing [[Writable]] of unconfigurable property not allowed");
      }
      return false;
    }
  }
  return true;
}

// section 15.4.5.1 step 3
inline bool JSArray::DefineLengthProperty(Context* ctx,
                                          const PropertyDescriptor& desc,
                                          bool throwable, Error* e) {
  if (desc.IsConfigurable()) {
    if (throwable) {
      e->Report(Error::Type, "changing [[Configurable]] of unconfigurable property not allowed");
    }
    return false;
  }

  if (desc.IsEnumerable()) {
    if (throwable) {
      e->Report(Error::Type, "changing [[Enumerable]] of unconfigurable property not allowed");
    }
    return false;
  }

  if (desc.IsAccessor()) {
    if (throwable) {
      e->Report(Error::Type, "changing descriptor type of unconfigurable property not allowed");
    }
    return false;
  }

  if (desc.IsValueAbsent()) {
    if (!desc.IsWritableAbsent()) {
      return ChangeLengthWritable(desc.IsWritable(), throwable, e);
    }
    return true;
  }

  const double new_len_double =
      desc.AsDataDescriptor()->value().ToNumber(ctx, IV_LV5_ERROR(e));
  // length must be uint32_t
  const uint32_t new_len = core::DoubleToUInt32(new_len_double);
  if (new_len != new_len_double) {
    // Important:
    // range error occurs even if throwable is false
    e->Report(Error::Range, "invalid array length");
    return false;
  }

  const uint32_t old_len = length();

  if (new_len == old_len) {
    // no change.
    // pass even if writable is false.
    if (!desc.IsWritableAbsent()) {
      return ChangeLengthWritable(desc.IsWritable(), throwable, e);
    }
    return true;
  }

  if (!elements_.writable()) {
    if (throwable) {
      e->Report(Error::Type, "\"length\" not writable");
    }
    return false;
  }

  const bool succeeded = SetLength(ctx, new_len, throwable, e);
  if (!desc.IsWritableAbsent()) {
    return ChangeLengthWritable(desc.IsWritable(), throwable, e);
  }
  return succeeded;
}

inline bool JSArray::SetLength(Context* ctx, uint32_t len, bool throwable, Error* e) {
  assert(elements_.writable());
  uint32_t old = length();
  if (len >= old) {
    elements_.set_length(len);
    return true;
  }

  // dense array shrink
  if (elements_.dense()) {
    // dense array version
    if (len > IndexedElements::kMaxVectorSize) {
      if (elements_.map) {
        SparseArrayMap* map = elements_.map;
        std::vector<uint32_t> copy(map->size());
        std::transform(
            map->begin(), map->end(),
            copy.begin(),
            [](const SparseArrayMap::value_type& pair) {
          return pair.first;
        });
        std::sort(copy.begin(), copy.end(), std::greater<uint32_t>());
        for (std::vector<uint32_t>::const_iterator it = copy.begin(),
             last = copy.end(); it != last; ++it) {
          if (*it >= len) {
            map->erase(*it);
          } else {
            break;
          }
        }
        if (map->empty()) {
          elements_.MakeDense();
        }
      }
    } else {
      elements_.MakeDense();
      if (elements_.vector.size() > len) {
        elements_.vector.resize(len, JSEmpty);
      }
    }
    elements_.set_length(len);
    return true;
  }

  // small shrink
  if ((old - len) < (1 << 24)) {
    while (len < old) {
      old -= 1;
      if (!JSObject::DeleteIndexedInternal(ctx, old, false, e)) {
        elements_.set_length(old + 1);
        if (throwable) {
          e->Report(Error::Type, "shrink array failed");
        }
        return false;
      }
    }
    elements_.set_length(len);
    return true;
  }

  // big shrink
  PropertyNamesCollector collector;
  JSObject::GetOwnPropertyNames(ctx, &collector, INCLUDE_NOT_ENUMERABLE);
  for (PropertyNamesCollector::Names::const_reverse_iterator
       it = collector.names().rbegin(),
       last = collector.names().rend();
       it != last; ++it) {
    if (!symbol::IsArrayIndexSymbol(*it)) {
      continue;
    }

    const uint32_t index = symbol::GetIndexFromSymbol(*it);
    if (!JSObject::DeleteIndexedInternal(ctx, index, false, e)) {
      elements_.set_length(index + 1);
      if (throwable) {
        e->Report(Error::Type, "shrink array failed");
      }
      return false;
    }
  }
  elements_.set_length(len);
  return true;
}

} }  // namespace iv::lv5
#endif  // IV_LV5_JSARRAY_H_
