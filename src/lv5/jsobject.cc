#include <cassert>
#include <algorithm>
#include "jsobject.h"
#include "property.h"
#include "jsfunction.h"
#include "jsval.h"
#include "jsenv.h"
#include "context.h"
#include "class.h"

namespace iv {
namespace lv5 {

JSObject::JSObject()
  : prototype_(NULL),
    class_name_(),
    extensible_(true),
    table_() {
}

JSObject::JSObject(JSObject* proto,
                   Symbol class_name,
                   bool extensible)
  : prototype_(proto),
    class_name_(class_name_),
    extensible_(extensible),
    table_() {
}

#define TRY(context, sym, arg, error)\
  do {\
    JSVal method = Get(context, sym, error);\
    if (*error) {\
      return JSUndefined;\
    }\
    if (method.IsCallable()) {\
      JSVal val = method.object()->AsCallable()->Call(arg, error);\
      if (*error) {\
        return JSUndefined;\
      }\
      if (val.IsPrimitive() || val.IsNull() || val.IsUndefined()) {\
        return val;\
      }\
    }\
  } while (0)
JSVal JSObject::DefaultValue(Context* ctx,
                             Hint::Object hint, Error* res) {
  const Arguments args(ctx, this);
  if (hint == Hint::STRING) {
    // hint is STRING
    TRY(ctx, ctx->toString_symbol(), args, res);
    TRY(ctx, ctx->valueOf_symbol(), args, res);
  } else {
    // section 8.12.8
    // hint is NUMBER or NONE
    TRY(ctx, ctx->valueOf_symbol(), args, res);
    TRY(ctx, ctx->toString_symbol(), args, res);
  }
  res->Report(Error::Type, "invalid default value");
  return JSUndefined;
}
#undef TRY

JSVal JSObject::Get(Context* ctx,
                    Symbol name, Error* res) {
  const PropertyDescriptor desc = GetProperty(name);
  if (desc.IsEmpty()) {
    return JSUndefined;
  }
  if (desc.IsDataDescriptor()) {
    return desc.AsDataDescriptor()->value();
  } else {
    assert(desc.IsAccessorDescriptor());
    JSObject* const getter = desc.AsAccessorDescriptor()->get();
    if (getter) {
      return getter->AsCallable()->Call(Arguments(ctx, this), res);
    } else {
      return JSUndefined;
    }
  }
}

// not recursion
PropertyDescriptor JSObject::GetProperty(Symbol name) const {
  const JSObject* obj = this;
  do {
    const PropertyDescriptor prop = obj->GetOwnProperty(name);
    if (!prop.IsEmpty()) {
      return prop;
    }
    obj = obj->prototype();
  } while (obj);
  return JSUndefined;
}

PropertyDescriptor JSObject::GetOwnProperty(Symbol name) const {
  const Properties::const_iterator it = table_.find(name);
  if (it == table_.end()) {
    return JSUndefined;
  } else {
    return it->second;
  }
}

bool JSObject::CanPut(Symbol name) const {
  const PropertyDescriptor desc = GetOwnProperty(name);
  if (!desc.IsEmpty()) {
    if (desc.IsAccessorDescriptor()) {
      return desc.AsAccessorDescriptor()->set();
    } else {
      assert(desc.IsDataDescriptor());
      return desc.AsDataDescriptor()->IsWritable();
    }
  }
  if (!prototype_) {
    return extensible_;
  }
  const PropertyDescriptor inherited = prototype_->GetProperty(name);
  if (inherited.IsEmpty()) {
    return extensible_;
  } else {
    if (inherited.IsAccessorDescriptor()) {
      return inherited.AsAccessorDescriptor()->set();
    } else {
      assert(inherited.IsDataDescriptor());
      return inherited.AsDataDescriptor()->IsWritable();
    }
  }
}

#define REJECT(str)\
  do {\
    if (th) {\
      res->Report(Error::Type, str);\
    }\
    return false;\
  } while (0)

bool JSObject::DefineOwnProperty(Context* ctx,
                                 Symbol name,
                                 const PropertyDescriptor& desc,
                                 bool th,
                                 Error* res) {
  // section 8.12.9 [[DefineOwnProperty]]
  const PropertyDescriptor current = GetOwnProperty(name);
  if (current.IsEmpty()) {
    if (!extensible_) {
      REJECT("object not extensible");
    } else {
      if (!desc.IsAccessorDescriptor()) {
        assert(desc.IsDataDescriptor() || desc.IsGenericDescriptor());
        table_[name] = PropertyDescriptor::SetDefault(desc);
      } else {
        assert(desc.IsAccessorDescriptor());
        table_[name] = PropertyDescriptor::SetDefault(desc);
      }
      return true;
    }
  }

  // step 5
  if (PropertyDescriptor::IsAbsent(desc)) {
    return true;
  }
  // step 6
  if (PropertyDescriptor::Equals(desc, current)) {
    return true;
  }

  // step 7
  if (!current.IsConfigurable()) {
    if (desc.IsConfigurable()) {
      REJECT(
          "changing [[Configurable]] of unconfigurable property not allowed");
    }
    if (!desc.IsEnumerableAbsent() &&
        current.IsEnumerable() != desc.IsEnumerable()) {
      REJECT("changing [[Enumerable]] of unconfigurable property not allowed");
    }
  }

  // step 9
  if (desc.IsGenericDescriptor()) {
    // no further validation
  } else if (current.type() != desc.type()) {
    if (!current.IsConfigurable()) {
      REJECT("changing descriptor type of unconfigurable property not allowed");
    }
    if (current.IsDataDescriptor()) {
      assert(desc.IsAccessorDescriptor());
    } else {
      assert(desc.IsDataDescriptor());
    }
  } else {
    // step 10
    if (current.IsDataDescriptor()) {
      assert(desc.IsDataDescriptor());
      if (!current.IsConfigurable()) {
        if (!current.AsDataDescriptor()->IsWritable()) {
          const DataDescriptor* const data = desc.AsDataDescriptor();
          if (data->IsWritable()) {
            REJECT(
                "changing [[Writable]] of unconfigurable property not allowed");
          }
          if (SameValue(current.AsDataDescriptor()->value(),
                        data->value())) {
            REJECT("changing [[Value]] of readonly property not allowed");
          }
        }
      }
    } else {
      // step 11
      assert(desc.IsAccessorDescriptor());
      if (!current.IsConfigurableAbsent() && !current.IsConfigurable()) {
        const AccessorDescriptor* const lhs = current.AsAccessorDescriptor();
        const AccessorDescriptor* const rhs = desc.AsAccessorDescriptor();
        if (lhs->set() != rhs->set() || lhs->get() != rhs->get()) {
          REJECT("changing [[Set]] or [[Get]] "
                 "of unconfigurable property not allowed");
        }
      }
    }
  }
  table_[name] = PropertyDescriptor::Merge(desc, current);
  return true;
}

#undef REJECT

void JSObject::Put(Context* ctx,
                   Symbol name,
                   const JSVal& val, bool th, Error* res) {
  if (!CanPut(name)) {
    if (th) {
      res->Report(Error::Type, "put failed");
    }
    return;
  }
  const PropertyDescriptor own_desc = GetOwnProperty(name);
  if (!own_desc.IsEmpty() && own_desc.IsDataDescriptor()) {
    DefineOwnProperty(ctx,
                      name,
                      DataDescriptor(
                          val,
                          PropertyDescriptor::UNDEF_ENUMERABLE |
                          PropertyDescriptor::UNDEF_CONFIGURABLE |
                          PropertyDescriptor::UNDEF_WRITABLE), th, res);
    return;
  }
  const PropertyDescriptor desc = GetProperty(name);
  if (!desc.IsEmpty() && desc.IsAccessorDescriptor()) {
    const AccessorDescriptor* const accs = desc.AsAccessorDescriptor();
    assert(accs->set());
    Arguments args(ctx, 1);
    args.set_this_binding(this);
    args[0] = val;
    accs->set()->AsCallable()->Call(args, res);
  } else {
    DefineOwnProperty(ctx, name,
                      DataDescriptor(val,
                                     PropertyDescriptor::WRITABLE |
                                     PropertyDescriptor::ENUMERABLE |
                                     PropertyDescriptor::CONFIGURABLE),
                      th, res);
  }
}

bool JSObject::HasProperty(Symbol name) const {
  return !GetProperty(name).IsEmpty();
}

bool JSObject::Delete(Symbol name, bool th, Error* res) {
  const PropertyDescriptor desc = GetOwnProperty(name);
  if (desc.IsEmpty()) {
    return true;
  }
  if (desc.IsConfigurable()) {
    table_.erase(name);
    return true;
  } else {
    if (th) {
      res->Report(Error::Type, "delete failed");
    }
    return false;
  }
}

void JSObject::GetPropertyNames(std::vector<Symbol>* vec,
                                EnumerationMode mode) const {
  using std::find;
  GetOwnPropertyNames(vec, mode);
  const JSObject* obj = prototype_;
  while (obj) {
    obj->GetOwnPropertyNames(vec, mode);
    obj = obj->prototype();
  }
}

void JSObject::GetOwnPropertyNames(std::vector<Symbol>* vec,
                                   EnumerationMode mode) const {
  using std::find;
  if (vec->empty()) {
    for (JSObject::Properties::const_iterator it = table_.begin(),
         last = table_.end(); it != last; ++it) {
      if (it->second.IsEnumerable() || (mode == kIncludeNotEnumerable)) {
        vec->push_back(it->first);
      }
    }
  } else {
    for (JSObject::Properties::const_iterator it = table_.begin(),
         last = table_.end(); it != last; ++it) {
      if ((it->second.IsEnumerable() || (mode == kIncludeNotEnumerable)) &&
          (find(vec->begin(), vec->end(), it->first) == vec->end())) {
        vec->push_back(it->first);
      }
    }
  }
}

JSObject* JSObject::New(Context* ctx) {
  JSObject* const obj = NewPlain(ctx);
  const Symbol name = ctx->Intern("Object");
  const Class& cls = ctx->Cls(name);
  obj->set_class_name(cls.name);
  obj->set_prototype(cls.prototype);
  return obj;
}

JSObject* JSObject::NewPlain(Context* ctx) {
  return new JSObject();
}

JSStringObject* JSStringObject::New(Context* ctx, JSString* str) {
  JSStringObject* const obj = new JSStringObject(str);
  const Symbol name = ctx->Intern("String");
  const Class& cls = ctx->Cls(name);
  obj->set_class_name(cls.name);
  obj->set_prototype(cls.prototype);
  obj->DefineOwnProperty(ctx, ctx->length_symbol(),
                         DataDescriptor(str->size(),
                                        PropertyDescriptor::NONE),
                                        false, ctx->error());
  return obj;
}

JSStringObject* JSStringObject::NewPlain(Context* ctx) {
  return new JSStringObject(JSString::NewEmptyString(ctx));
}

JSNumberObject* JSNumberObject::New(Context* ctx, const double& value) {
  JSNumberObject* const obj = new JSNumberObject(value);
  const Class& cls = ctx->Cls("Number");
  obj->set_class_name(cls.name);
  obj->set_prototype(cls.prototype);
  return obj;
}

JSNumberObject* JSNumberObject::NewPlain(Context* ctx, const double& value) {
  return new JSNumberObject(value);
}

JSBooleanObject::JSBooleanObject(bool value)
  : JSObject(),
    value_(value) {
}

JSBooleanObject* JSBooleanObject::NewPlain(Context* ctx, bool value) {
  return new JSBooleanObject(value);
}

JSBooleanObject* JSBooleanObject::New(Context* ctx, bool value) {
  JSBooleanObject* const obj = new JSBooleanObject(value);
  const Class& cls = ctx->Cls(ctx->Intern("Boolean"));
  obj->set_class_name(cls.name);
  obj->set_prototype(cls.prototype);
  return obj;
}

} }  // namespace iv::lv5
