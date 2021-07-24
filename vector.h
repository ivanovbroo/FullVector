#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory { 
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

    const T* Begin() const {
        return buffer_;
    }
    T* Begin() {
        return buffer_;
    }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;

    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }
};

template <typename T>
class Vector {
private:
    RawMemory<T> data_;
    size_t size_ = 0;

public:
    Vector() = default;

    /*
    *   Этот конструктор сначала выделяет в сырой памяти буфер,
    *   достаточный для хранения элементов в количестве, равном size.
    *   Затем конструирует в сырой памяти элементы массива.
    *   Для этого он вызывает их конструктор по умолчанию, используя размещающий оператор new
    */
    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    /*
    *   Чтобы создать копию контейнера Vector, выделим память под нужное количество элементов,
    *   а затем сконструируем в ней копию элементов оригинального контейнера,
    *   используя функцию CopyConstruct
    *   Здесь вместимость копии равна размеру оригинального вектора.
    *   Это экономит память: независимо от вместимости оригинального вектора копия будет занимать столько памяти,
    *   сколько нужно для хранения его элементов.
    */
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    /*  Перемещающий конструктор. Выполняется за O(1) и не выбрасывает исключений. */
    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    /*
    *   Оператор копирующего присваивания.
    *   Выполняется за O(N), где N — максимум из размеров векторов, участвующих в операции.
    */
    Vector& operator=(const Vector& rhs) {
        if (rhs.size_ > data_.Capacity()) {
            Vector<T> tmp(rhs);
            Swap(tmp);
        }
        else {
            if (size_ < rhs.size_) {
                std::uninitialized_copy_n(
                    rhs.data_.GetAddress() + size_,
                    rhs.size_ - size_,
                    data_.GetAddress() + size_
                );
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
            }

            if (size_ > rhs.size_) {
                
                std::destroy_n(
                    data_.GetAddress() + rhs.size_,
                    size_ - rhs.size_
                );
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
            }
            size_ = rhs.size_;
        }
        return *this;
    }

    /*  Оператор перемещающего присваивания. Выполняется за O(1) и не выбрасывает исключений. */
    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    /*
    *   Метод Swap, выполняющий обмен содержимого вектора с другим вектором.
    *   Операция должна иметь сложность O(1) и не выбрасывать исключений.
    */
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    /*
    *   Метод Reserve предназначен для заблаговременного резервирования памяти под элементы вектора,
    *   когда известно их примерное количество.
    *   Резервирует достаточно места, чтобы вместить количество элементов, равное capacity.
    *   Если новая вместимость не превышает текущую, метод не делает ничего. Алгоритмическая сложность: O(размер вектора).
    */
    void Reserve(size_t new_capacity) {

        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        //Да, в теории объясняется для чего используется перемещение и копирование
        /*  Перемещайте элементы, только если соблюдается хотя бы одно из условий:
            конструктор перемещения типа T не выбрасывает исключений;
            тип T не имеет копирующего конструктора.
            В противном случае элементы надо копировать. */
        //В теории сам автор предлагает такое решение, я попробовал убрать копирование и соответсвенно разрушение элементов,
        //увы тесты не проходят
        //тогда и правда задается вопрос, если происходит перемещение, зачем нам тогда разрушать элементы?
        //попробовал перенести разрешение элементов, где копирование, снова тесты не проходят
        //как в этом случае быть?
        

        /// могу ошибаться, возможно условия задачи такие хитрый, но если подойти логически, то при резервировании значения в старом векторе будут не нужны,
        /// т.е. правильней всегда делать перемещение
                /*
                *   Перемещайте элементы, только если соблюдается хотя бы одно из условий:
                *      1) конструктор перемещения типа T не выбрасывает исключений;
                *      2) тип T не имеет копирующего конструктора.
                *   В противном случае элементы надо копировать.
                *   Шаблоны std::is_copy_constructible_v и std::is_nothrow_move_constructible_v помогают узнать,
                *   есть ли у типа копирующий конструктор и noexcept-конструктор перемещения.
                */
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            // Конструируем элементы в new_data, перемещая их из data_
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            // Конструируем элементы в new_data, копируя их из data_
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        // Разрушаем элементы в data_
/// а если было перемещение, то что должно удаляться?
        std::destroy_n(data_.GetAddress(), size_);
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
        // При выходе из метода старая память будет возвращена в кучу
    }

    /*  Для корректного разрушения контейнера Vector нужно сначала вызвать DestroyN,
    *   передав ей указатель data_ и количество элементов size_,
    *   а затем Deallocate, чтобы вернуть память обратно в кучу
    *   Разрушает содержащиеся в векторе элементы и освобождает занимаемую ими память.
    *   Алгоритмическая сложность: O(размер вектора).
    */
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    /*
    *   В константном операторе [] используется оператор [const_cast],
    *   чтобы снять константность с ссылки на текущий объект и вызвать неконстантную версию оператора [].
    *   Так получится избавиться от дублирования проверки assert(index < size).
    *   Оператор const_cast позволяет сделать то, что нельзя, но, если очень хочется, можно.
    *   В данном случае нельзя вызвать неконстантный метод из константного.
    *   Но неконстантный оператор [] тут не модифицирует состояние объекта, поэтому его можно вызвать,
    *   предварительно сняв константность с объекта.
    */
    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    //  Метод Resize изменяет количество элементов в векторе
    void Resize(size_t new_size) {
        Reserve(new_size);
        if (size_ < new_size) {
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        else if (size_ > new_size) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    /*
    *   Метод PushBack добавляет новое значение в конец вектора.
    *   При нехватке памяти стандартный vector увеличивает вместимость в кратное число раз.
    */
    template <typename Type> void PushBack(Type&& value)
    {
        EmplaceBack(std::forward<Type>(value));
    } 

    /*
    *   Метод PopBack разрушает последний элемент вектора и уменьшает размер вектора на единицу.
    *   Как и в случае стандартного вектора, вызов PopBack на пустом векторе приводит к неопределённому поведению.
    */
    void PopBack() noexcept {
        std::destroy_at(data_ + size_ - 1);
        --size_;
    }

    /*
    *   Метод EmplaceBack должен уметь вызывать любые конструкторы типа T,
    *   передавая им как константные и неконстантные ссылки, так и rvalue-ссылки на временные объекты.
    *   Для этого EmplaceBack должен быть вариативным шаблоном,
    *   который принимает аргументы конструктора T по Forwarding-ссылкам.
    *   Метод EmplaceBack, добавляющий новый элемент в конец вектора.
    */
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {

        if (data_.Capacity() > Size()) {
            auto tmp = new (data_.GetAddress() + Size()) T(std::forward<Args>(args)...);
            ++size_;
            return *tmp;
        }
        else {
            /// изменять вместимость желательно только одним методом Reserve

            //немного не понимаю, Reserve резервирует память для старого вектора памяти + метод типа void
            //как тогда применить метод Reserve для new_data? 
            //В теории говорится, что нужно создать новую сырую область памяти и с ней работать
            //или как-то можно по другому воспользоваться Reserve для new_data?
            
            RawMemory<T> new_data(Size() == 0 ? 1 : Size() * 2);
            auto tmp = new (new_data.GetAddress() + Size()) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                // Конструируем элементы в new_data, перемещая их из data_
                std::uninitialized_move_n(data_.GetAddress(), Size(), new_data.GetAddress());
            }
            else {
                // Конструируем элементы в new_data, копируя их из data_
                std::uninitialized_copy_n(data_.GetAddress(), Size(), new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), Size());
            data_.Swap(new_data);
            ++size_;
            return *tmp;
        }
    }

    /* ИТЕРАТОРЫ */

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.Begin();
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator begin() const noexcept {
        return data_.Begin();
    }

    const_iterator end() const noexcept {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.Begin();
    }

    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    /* КОНЕЦ ИТЕРАТОРОВ */

    //  Метод Emplace вставляет элемент в заданную позицию вектора.
/// рекомендую упростить метод, в таком виде он сложен для понимания, может выделить часть кода в отдельный метод
/// может получится для изменения вместимости использовать Reserve, будет немного менее эффективно, но понятнее
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        const size_t offset = pos - data_.GetAddress();

        if (data_.Capacity() > Size()) { 
            BigCapacity(pos, std::forward<Args>(args)...);
        }
        else {
            CompletelyFilled(pos, std::forward<Args>(args)...);           
        }
        return begin() + offset;
    }

    iterator Erase(const_iterator pos) {
        if (pos == end()) {
            return end();
        }

        auto position_elemet = begin() + (pos - cbegin());      

        std::move(position_elemet + 1, end(), position_elemet);

        std::destroy_at(end() - 1);

        --size_;
        return position_elemet;
    }

    //  Метод Insert вставляет элемент в заданную позицию вектора
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    //  Метод Insert вставляет элемент в заданную позицию вектора
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));

    }

private:
    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

    template <typename... Args>
    void BigCapacity(const_iterator pos, Args&&... args) {
        const size_t offset = pos - data_.GetAddress();

        //вычисляем позицию вставки элемента в памяти
        auto position_elemet = begin() + offset;

        if (position_elemet == end()) {
            EmplaceBack(std::forward<Args>(args)...);
        }
        else {
            T tmp_new_elem{ std::forward<Args>(args)... };

            // перемещаем последний элемент = (*(end() - 1)
            // начало диапазона назначения = data_ + size_
            new (data_ + size_) T(std::move(*(end() - 1)));

            //перемещаем элементы на один элемент вправо
            std::move_backward(position_elemet, end() - 1, data_ + size_);

            //перемещаем элемент наш временный элемент
            *position_elemet = std::move(tmp_new_elem);
            ++size_;
        }
    }

    template <typename... Args>
    void CompletelyFilled(const_iterator pos, Args&&... args) {
        const size_t offset = pos - data_.GetAddress();

        RawMemory<T> new_data(Size() == 0 ? 1 : Size() * 2);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            // Конструируем элементы в new_data, перемещая их из data_
            std::uninitialized_move_n(data_.GetAddress(), offset, new_data.GetAddress());
        }
        else {
            //убрал копирование, терминал не пропускает такое решение
            //немного не пойму почему не нужны значения в старом векторе?
            //мы же их либо перемещаем или копируем в определенную ячейку памяти,
            //далее просто свапаем с новой сырой памятью
            //и элементы в старой памяти автоматически удаляются
            // Конструируем элементы в new_data, копируя их из data_
            try {
                /// зачем копирование? вроде как значения в старом векторе будут не нужны
                std::uninitialized_copy_n(data_.GetAddress(), offset, new_data.GetAddress());
            }
            catch (...) {
                std::destroy_n(new_data + offset, 1);
                throw;
            }
        }
        //создаем в указанной позиции вектора элемент
        new (new_data + offset) T(std::forward<Args>(args)...);
        if (size_ > offset) {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                // Конструируем элементы в new_data, перемещая их из data_
                std::uninitialized_move_n(
                    data_.GetAddress() + offset,
                    size_ - offset,
                    new_data.GetAddress() + offset + 1);
            }
            else {
                // Конструируем элементы в new_data, копируя их из data_
                try {
                    //убрал копирование, терминал не пропускает такое решение

                    /// зачем копирование? вроде как значения в старом векторе будут не нужны
                    std::uninitialized_copy_n(
                        data_.GetAddress() + offset,
                        size_ - offset,
                        new_data.GetAddress() + offset + 1);
                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress(), offset + 1);
                    throw;
                }

            }
        }
        /// а если было перемещение, что будет разрушать destroy_n ?
        std::destroy_n(data_.GetAddress(), Size());
        data_.Swap(new_data);
        ++size_;
    }
};