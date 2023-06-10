struct Node::UserData
{
  const std::type_info& type_info;

  UserData(const std::type_info& type_info)
    : type_info(type_info)
  {
  }
};

template <class T>
struct Node::ConcreteUserData : UserData
{
  T value;

  ConcreteUserData(const T& value)
    : UserData(typeid(T))
    , value(value)
  {
  }
};

template <class T>
T& Node::set_user_data(const T& value)
{
  static const std::type_info& type = typeid(T);

  if (UserDataPtr user_data = find_user_data_core(type))
  {
    auto* casted_ptr = static_cast<ConcreteUserData<T>*>(&*user_data);
    casted_ptr->value = value;
    return casted_ptr->value;
  }

  auto ptr = std::make_shared<ConcreteUserData<T>>(value);

  set_user_data_core(type, ptr);

  return ptr->value;
}

template <class T>
void Node::reset_user_data()
{
  static const std::type_info& type = typeid(T);

  set_user_data_core(type, nullptr);
}

template <class T>
T* Node::find_user_data() const
{
  static const std::type_info& type = typeid(T);  

  UserDataPtr user_data = find_user_data_core(type);

  if (!user_data)
    return nullptr;

  auto* casted_ptr = static_cast<ConcreteUserData<T>*>(&*user_data);

  return &casted_ptr->value;
}

template <class T>
T& Node::get_user_data() const
{
  if (T* value = find_user_data<T>())
    return value;

  throw common::Exception::format("No user data of type '%s' bound to node", typeid(T).name());
}
