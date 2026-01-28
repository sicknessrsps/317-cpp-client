#pragma once
#include <algorithm>
#include <utility>
#include <vector>

namespace SDL_Client
{
    //
    // ======== Base 2D Array ========
    //
    template<typename T>
    class Array2D {
    public:
        Array2D() = default;
        Array2D(size_t dim1, size_t dim2)
            : dim1_(dim1), dim2_(dim2), data_(dim1 * dim2) {}

        void resize(size_t dim1, size_t dim2) {
            dim1_ = dim1;
            dim2_ = dim2;
            data_.resize(dim1 * dim2);
        }

        inline T& operator()(size_t i, size_t j) {
            return data_[i * dim2_ + j];
        }

        inline const T& operator()(size_t i, size_t j) const {
            return data_[i * dim2_ + j];
        }

        inline size_t dim1() const { return dim1_; }
        inline size_t dim2() const { return dim2_; }
        inline T* data() { return data_.data(); }
        inline const T* data() const { return data_.data(); }

        inline void fill(const T& value) {
            std::fill(data_.begin(), data_.end(), value);
        }

        inline void fill() {
            for (auto& elem : data_) {
                elem = T{};
            }
        }

        template<typename F>
        inline void forEach(F&& func) {
            for (auto& elem : data_) {
                func(elem);
            }
        }

        template<typename F>
        inline void forEach(F&& func) const {
            for (const auto& elem : data_) {
                func(elem);
            }
        }

    private:
        size_t dim1_ = 0, dim2_ = 0;
        std::vector<T> data_;
    };

    //
    // ======== Proxy for [] 2D Access ========
    //
    template<typename T>
    class Array2DProxyRow {
    public:
        Array2DProxyRow(T* base, size_t dim2) : base_(base), dim2_(dim2) {}

        inline T& operator[](size_t j) { return base_[j]; }
        inline const T& operator[](size_t j) const { return base_[j]; }

    private:
        T* base_;
        size_t dim2_;
    };

    template<typename T>
    class Array2DIndexed {
    public:
        Array2DIndexed(size_t x = 0, size_t y = 0) : arr_(x, y) {}

        inline Array2DProxyRow<T> operator[](size_t i) {
            return Array2DProxyRow<T>(arr_.data() + i * arr_.dim2(), arr_.dim2());
        }

        inline const Array2DProxyRow<T> operator[](size_t i) const {
            return Array2DProxyRow<T>(arr_.data() + i * arr_.dim2(), arr_.dim2());
        }

        inline void resize(size_t x, size_t y) { arr_.resize(x, y); }

        inline size_t dim1() const { return arr_.dim1(); }
        inline size_t dim2() const { return arr_.dim2(); }

        inline T* data() { return arr_.data(); }
        inline const T* data() const { return arr_.data(); }

        inline void fill(const T& value) { arr_.fill(value); }
        inline void fill() { arr_.fill(); }

        template<typename F>
        inline void forEach(F&& func) { arr_.forEach(std::forward<F>(func)); }

        template<typename F>
        inline void forEach(F&& func) const { arr_.forEach(std::forward<F>(func)); }

    private:
        Array2D<T> arr_;
    };

    // Non-owning 2D view (header-only)
    template<typename T>
    class Array2DView {
    public:
        Array2DView() = default;
        Array2DView(T* base, size_t dim1, size_t dim2)
            : base_(base), dim1_(dim1), dim2_(dim2) {}

        inline Array2DProxyRow<T> operator[](size_t i) {
            return Array2DProxyRow<T>(base_ + i * dim2_, dim2_);
        }
        inline const Array2DProxyRow<T> operator[](size_t i) const {
            return Array2DProxyRow<T>(base_ + i * dim2_, dim2_);
        }

        inline size_t dim1() const { return dim1_; }
        inline size_t dim2() const { return dim2_; }
        inline T* data() { return base_; }

    private:
        T* base_ = nullptr;
        size_t dim1_ = 0;
        size_t dim2_ = 0;
    };

    //
    // ======== Base 3D Array ========
    //
    template<typename T>
    class Array3D {
    public:
        Array3D() = default;
        Array3D(size_t dim1, size_t dim2, size_t dim3)
            : dim1_(dim1), dim2_(dim2), dim3_(dim3), data_(dim1 * dim2 * dim3) {}

        void resize(size_t dim1, size_t dim2, size_t dim3) {
            dim1_ = dim1;
            dim2_ = dim2;
            dim3_ = dim3;
            data_.resize(dim1 * dim2 * dim3);
        }

        inline T& operator()(size_t i, size_t j, size_t k) {
            return data_[i * dim2_ * dim3_ + j * dim3_ + k];
        }

        inline const T& operator()(size_t i, size_t j, size_t k) const {
            return data_[i * dim2_ * dim3_ + j * dim3_ + k];
        }

        inline size_t dim1() const { return dim1_; }
        inline size_t dim2() const { return dim2_; }
        inline size_t dim3() const { return dim3_; }
        inline T* data() { return data_.data(); }
        inline const T* data() const { return data_.data(); }

        inline void fill(const T& value) {
            std::fill(data_.begin(), data_.end(), value);
        }

        inline void fill() {
            for (auto& elem : data_) {
                elem = T{};
            }
        }

        template<typename F>
        inline void forEach(F&& func) {
            for (auto& elem : data_) {
                func(elem);
            }
        }

        template<typename F>
        inline void forEach(F&& func) const {
            for (const auto& elem : data_) {
                func(elem);
            }
        }

    private:
        size_t dim1_ = 0, dim2_ = 0, dim3_ = 0;
        std::vector<T> data_;
    };

    //
    // ======== Proxy for [][][] Access ========
    //
    template<typename T>
    class Array3DProxyRow {
    public:
        Array3DProxyRow(T* base, size_t dim3) : base_(base), dim3_(dim3) {}
        inline T& operator[](size_t z) { return base_[z]; }
        inline const T& operator[](size_t z) const { return base_[z]; }
    private:
        T* base_;
        size_t dim3_;
    };

    template<typename T>
    class Array3DProxyLevel {
    public:
        Array3DProxyLevel(T* base, size_t dim2, size_t dim3)
            : base_(base), dim2_(dim2), dim3_(dim3) {}

        inline Array3DProxyRow<T> operator[](size_t x) {
            return Array3DProxyRow<T>(base_ + x * dim3_, dim3_);
        }

        inline const Array3DProxyRow<T> operator[](size_t x) const {
            return Array3DProxyRow<T>(base_ + x * dim3_, dim3_);
        }

    private:
        T* base_;
        size_t dim2_, dim3_;
    };

    template<typename T>
    class Array3DIndexed {
    public:
        Array3DIndexed(size_t l = 0, size_t x = 0, size_t z = 0) : arr_(l, x, z) {}

        inline Array3DProxyLevel<T> operator[](size_t level) {
            return Array3DProxyLevel<T>(
                arr_.data() + level * arr_.dim2() * arr_.dim3(),
                arr_.dim2(), arr_.dim3());
        }

        inline const Array3DProxyLevel<T> operator[](size_t level) const {
            return Array3DProxyLevel<T>(
                arr_.data() + level * arr_.dim2() * arr_.dim3(),
                arr_.dim2(), arr_.dim3());
        }

        inline void resize(size_t l, size_t x, size_t z) { arr_.resize(l, x, z); }

        inline size_t dim1() const { return arr_.dim1(); }
        inline size_t dim2() const { return arr_.dim2(); }
        inline size_t dim3() const { return arr_.dim3(); }

        inline T* data() { return arr_.data(); }
        inline const T* data() const { return arr_.data(); }

        inline void fill(const T& value) { arr_.fill(value); }
        inline void fill() { arr_.fill(); }

        template<typename F>
        inline void forEach(F&& func) { arr_.forEach(std::forward<F>(func)); }

        template<typename F>
        inline void forEach(F&& func) const { arr_.forEach(std::forward<F>(func)); }

    private:
        Array3D<T> arr_;
    };

    template<typename T>
    class Array4D {
    public:
        Array4D() = default;
        Array4D(size_t d1, size_t d2, size_t d3, size_t d4)
            : d1_(d1), d2_(d2), d3_(d3), d4_(d4),
              data_(d1 * d2 * d3 * d4) {}

        void resize(size_t d1, size_t d2, size_t d3, size_t d4) {
            d1_ = d1; d2_ = d2; d3_ = d3; d4_ = d4;
            data_.resize(d1 * d2 * d3 * d4);
        }

        inline T& operator()(size_t i, size_t j, size_t k, size_t l) {
            return data_[((i * d2_ + j) * d3_ + k) * d4_ + l];
        }

        inline const T& operator()(size_t i, size_t j, size_t k, size_t l) const {
            return data_[((i * d2_ + j) * d3_ + k) * d4_ + l];
        }

        inline size_t dim1() const { return d1_; }
        inline size_t dim2() const { return d2_; }
        inline size_t dim3() const { return d3_; }
        inline size_t dim4() const { return d4_; }

        inline T* data() { return data_.data(); }
        inline const T* data() const { return data_.data(); }

        inline void fill(const T& value) {
            std::fill(data_.begin(), data_.end(), value);
        }

        inline void fill() {
            for (auto& elem : data_) {
                elem = T{};
            }
        }

        template<typename F>
        inline void forEach(F&& func) {
            for (auto& elem : data_) {
                func(elem);
            }
        }

        template<typename F>
        inline void forEach(F&& func) const {
            for (const auto& elem : data_) {
                func(elem);
            }
        }

    private:
        size_t d1_ = 0, d2_ = 0, d3_ = 0, d4_ = 0;
        std::vector<T> data_;
    };

    // ---- Proxy classes for [][] access ----

    template<typename T>
    class Array4DProxy3 {
    public:
        Array4DProxy3(T* base, size_t d3, size_t d4)
            : base_(base), d3_(d3), d4_(d4) {}

        class Array4DProxy2 {
        public:
            Array4DProxy2(T* base, size_t d4) : base_(base), d4_(d4) {}
            inline T& operator[](size_t y) { return base_[y]; }
            inline const T& operator[](size_t y) const { return base_[y]; }
        private:
            T* base_;
            size_t d4_;
        };

        inline Array4DProxy2 operator[](size_t x) {
            return Array4DProxy2(base_ + x * d4_, d4_);
        }

        inline const Array4DProxy2 operator[](size_t x) const {
            return Array4DProxy2(base_ + x * d4_, d4_);
        }

    private:
        T* base_;
        size_t d3_, d4_;
    };

    template<typename T>
    class Array4DProxy2 {
    public:
        Array4DProxy2(T* base, size_t d2, size_t d3, size_t d4)
            : base_(base), d2_(d2), d3_(d3), d4_(d4) {}

        inline Array4DProxy3<T> operator[](size_t j) {
            return Array4DProxy3<T>(base_ + j * d3_ * d4_, d3_, d4_);
        }

        inline const Array4DProxy3<T> operator[](size_t j) const {
            return Array4DProxy3<T>(base_ + j * d3_ * d4_, d3_, d4_);
        }

    private:
        T* base_;
        size_t d2_, d3_, d4_;
    };

    template<typename T>
    class Array4DIndexed {
    public:
        Array4DIndexed(size_t d1 = 0, size_t d2 = 0, size_t d3 = 0, size_t d4 = 0)
            : arr_(d1, d2, d3, d4) {}

        inline Array4DProxy2<T> operator[](size_t i) {
            return Array4DProxy2<T>(
                arr_.data() + i * arr_.dim2() * arr_.dim3() * arr_.dim4(),
                arr_.dim2(), arr_.dim3(), arr_.dim4());
        }

        inline const Array4DProxy2<T> operator[](size_t i) const {
            return Array4DProxy2<T>(
                arr_.data() + i * arr_.dim2() * arr_.dim3() * arr_.dim4(),
                arr_.dim2(), arr_.dim3(), arr_.dim4());
        }

        // Non-owning 2D view of slice [i][j]
        inline Array2DView<T> planeView(size_t i, size_t j) {
            // pointer to start of slice (i,j,0,0)
            T* base = arr_.data()
                     + i * arr_.dim2() * arr_.dim3() * arr_.dim4()
                     + j * arr_.dim3() * arr_.dim4();
            return Array2DView<T>(base, arr_.dim3(), arr_.dim4());
        }

        inline const Array2DView<T> planeView(size_t i, size_t j) const {
            const T* baseConst = arr_.data()
                     + i * arr_.dim2() * arr_.dim3() * arr_.dim4()
                     + j * arr_.dim3() * arr_.dim4();
            // const_cast ok for returning to const view overload; keep API consistent
            return Array2DView<T>(const_cast<T*>(baseConst), arr_.dim3(), arr_.dim4());
        }

        inline void resize(size_t d1, size_t d2, size_t d3, size_t d4) {
            arr_.resize(d1, d2, d3, d4);
        }

        inline void fill(const T& value) { arr_.fill(value); }
        inline void fill() { arr_.fill(); }

        template<typename F>
        inline void forEach(F&& func) { arr_.forEach(std::forward<F>(func)); }

        template<typename F>
        inline void forEach(F&& func) const { arr_.forEach(std::forward<F>(func)); }

    private:
        Array4D<T> arr_;
    };

    //
    // ======== Jagged Array with Packed Storage ========
    //
    template<typename T>
    class JaggedArray {
    public:
        JaggedArray() = default;

        // Initialize from vector of vectors
        JaggedArray(const std::vector<std::vector<T>>& data) {
            rows_.reserve(data.size());
            size_t total_elements = 0;

            // Calculate total size and store row info
            for (const auto& row : data) {
                rows_.push_back({total_elements, row.size()});
                total_elements += row.size();
            }

            // Flatten data
            storage_.reserve(total_elements);
            for (const auto& row : data) {
                storage_.insert(storage_.end(), row.begin(), row.end());
            }
        }

        // Returns a span-like view that acts like vector<T>
        class RowView {
        public:
            RowView(const T* data, size_t size) : data_(data), size_(size) {}

            inline const T& operator[](size_t idx) const { return data_[idx]; }
            inline size_t size() const { return size_; }
            inline const T* begin() const { return data_; }
            inline const T* end() const { return data_ + size_; }
            inline const T* data() const { return data_; }

        private:
            const T* data_;
            size_t size_;
        };

        inline RowView operator[](size_t row_idx) const {
            const auto& row_info = rows_[row_idx];
            return RowView(storage_.data() + row_info.offset, row_info.size);
        }

        inline size_t size() const { return rows_.size(); }

    private:
        struct RowInfo {
            size_t offset;
            size_t size;
        };

        std::vector<T> storage_;          // Flat packed storage
        std::vector<RowInfo> rows_;       // Row metadata
    };
}