/** \file Nifti-inl.h
 \brief Definitions of templated IO code for Nifti
 - Written by Tobias Wood, IoP KCL
 - Based on nifti1_io.h (Thanks to Robert Cox et al)
 - This code is released to the public domain. Do with it what you will.
 - This file should NOT be included directly in projects. It is just to 
 - separate the interface from the definition a bit better.
 */

#ifndef NIFTI_NIFTI_INL
#define NIFTI_NIFTI_INL

/*
 *   Simple class to apply the Nifti scaling (slope & intercept) and cast between the file datatype
 *   and the desired storage type. Has to be a templated class with static functions to support
 *   template specialisation, which in return is required to make this work with complex datatypes.
 *
 *   @param val   The actual value to scale and cast.
 *   @param slope Scaling slope.
 *   @param inter Scaling intercept.
 *
 */
template<typename FromTp, typename ToTp>
class Nifti::Scale{
	public:
	static ToTp Forward(const FromTp val, const float &slope, const float &inter) { return static_cast<ToTp>(val * slope + inter); }
	static ToTp Reverse(const FromTp val, const float &slope, const float &inter) { return static_cast<ToTp>((val - inter) / slope); }
};

template<typename FromTp, typename ToTp>
class Nifti::Scale<FromTp, std::complex<ToTp>> {
	public:
	static std::complex<ToTp> Forward(const FromTp val, const float &slope, const float &inter) { return std::complex<ToTp>(val * slope + inter, 0.); }
	static std::complex<ToTp> Reverse(const FromTp val, const float &slope, const float &inter) { return std::complex<ToTp>((val - inter) / slope, 0.); }
};

template<typename FromTp, typename ToTp>
class Nifti::Scale<std::complex<FromTp>, ToTp> {
	public:
	static ToTp Forward(const std::complex<FromTp> val, const float &slope, const float &inter) { return static_cast<ToTp>(std::abs(val) * slope + inter); }
	static ToTp Reverse(const std::complex<FromTp> val, const float &slope, const float &inter) { return static_cast<ToTp>((std::abs(val) - inter) / slope); }
};

template<typename FromTp, typename ToTp>
class Nifti::Scale<std::complex<FromTp>, std::complex<ToTp>> {
	public:
	static std::complex<ToTp> Forward(const std::complex<FromTp> val, const float &slope, const float &inter) { return static_cast<std::complex<ToTp>>(val * static_cast<FromTp>(slope) + static_cast<FromTp>(inter)); }
	static std::complex<ToTp> Reverse(const std::complex<FromTp> val, const float &slope, const float &inter) { return static_cast<std::complex<ToTp>>((val - static_cast<FromTp>(inter)) / static_cast<FromTp>(slope)); }
};

/*
 *   Core IO routine. Depending on the mode of the file, reads/writes a
 *   contiguous subregion of the region and converts to/from the desired datatype.
 *
 *   start and size can be short of the full number of dimensions, e.g. if you
 *   only want a part of the first volume in a multi-volume image, then they can
 *   just be 3D. An entry of 0 in size indicates that we want to read the whole dimension.
 *
 *   If start & size do not have the same number of rows out_of_range is thrown.
 *   If start or size are larger than the image dimensions out_of_range is thrown.
 *   If (start + size) exceeds any image dimensions out_of_range is thrown.
 *   If the storage size (end - begin) is not equal to the number of voxels to read, out_of_range is thrown.
 *
 *   @param start The voxel indices of the first desired voxel.
 *   @param size  The size of the desired subregion.
 *   @param begin Iterator to the beginning of data storge.
 *   @parem end   Iterator to the end of the data storage.
 */
template<typename Iter>
void Nifti::readWriteVoxels(const ArrayXs &start, const ArrayXs &inSize, Iter &storageBegin, Iter &storageEnd) {
	ArrayXs size = inSize;
	// 0 indicates read whole dimension, so swap for the real size
	for (ArrayXs::Index i = 0; i < size.rows(); i++)
		if (size(i) == 0) size(i) = m_dim(i);
	
	if (start.rows() != size.rows()) throw(std::out_of_range("Start and size must have same dimension in image: " + imagePath()));
	if (start.rows() > m_dim.rows()) throw(std::out_of_range("Too many read/write dimensions specified in image: " + imagePath()));
	if (((start + size) > m_dim.head(start.rows())).any()) throw(std::out_of_range("Read/write past image dimensions requested: " + imagePath()));
	if (size.prod() != std::distance(storageBegin, storageEnd)) throw(std::out_of_range("Storage size does not match requested read/write size in image: " + imagePath()));
	
	// Now collapse sequential reads/writes into the biggest read/write we can for efficiency
	size_t firstDim = 0; // We can always read first dimension in one go
	size_t blockSize = size(firstDim);
	while ((size(firstDim) == m_dim(firstDim)) && (firstDim < size.rows() - 1)) {
		firstDim++;
		blockSize *= size(firstDim);
	}
	std::vector<char> blockBytes(blockSize * m_typeinfo.size);
	ArrayXs blockStart = start;
	Iter blockIter{storageBegin};
	std::function<void ()> scaleAndCast = [&] () {

		// Helper macro. Templated lambdas would be very useful
		#define DECL_LOOP( TYPE )\
			auto bytePtr = reinterpret_cast<TYPE *>(blockBytes.data());\
			if (m_mode == Mode::Read) {\
				auto scale = Scale<TYPE, typename std::remove_reference<decltype(*blockIter)>::type>::Forward;\
				for (size_t el = 0; el < blockSize; el++, bytePtr++, blockIter++) { *blockIter = scale(*bytePtr, scaling_slope, scaling_inter); }\
			} else {\
				auto scale = Scale<typename std::remove_reference<decltype(*blockIter)>::type, TYPE>::Reverse;\
				for (size_t el = 0; el < blockSize; el++, bytePtr++, blockIter++) { *bytePtr = scale(*blockIter, scaling_slope, scaling_inter); }\
			}
		// End Helper Macro

		switch (m_typeinfo.type) {
			case DataType::INT8:       { DECL_LOOP(int8_t) }; break;
			case DataType::INT16:      { DECL_LOOP(int16_t) }; break;
			case DataType::INT32:      { DECL_LOOP(int32_t) }; break;
			case DataType::INT64:      { DECL_LOOP(int64_t) }; break;
			case DataType::UINT8:      { DECL_LOOP(uint8_t) }; break;
			case DataType::UINT16:     { DECL_LOOP(uint16_t) }; break;
			case DataType::UINT32:     { DECL_LOOP(uint32_t) }; break;
			case DataType::UINT64:     { DECL_LOOP(uint64_t) }; break;
			case DataType::FLOAT32:    { DECL_LOOP(float) }; break;
			case DataType::FLOAT64:    { DECL_LOOP(double) }; break;
			case DataType::FLOAT128:   { DECL_LOOP(long double) }; break;
			case DataType::COMPLEX64:  { DECL_LOOP(std::complex<float>) }; break;
			case DataType::COMPLEX128: { DECL_LOOP(std::complex<double>) }; break;
			case DataType::COMPLEX256: { DECL_LOOP(std::complex<long double>) }; break;
			case DataType::RGB24: case DataType::RGBA32:
				throw(std::runtime_error("Unsupported datatype.")); break;
		}
		#undef DECL_LOOP
	};

	// Read the next dimension. If we have reached the first collapsed dimension then read/write
	std::function<void (const size_t dim)> nextDim = [&] (const size_t dim) {
		if (dim == firstDim) {
			seekToVoxel(blockStart);
			if (m_mode == Mode::Read) readBytes(blockBytes);
			scaleAndCast();
			if (m_mode == Mode::Write) writeBytes(blockBytes);
		} else {
			for (size_t v = start(dim); v < start(dim) + size(dim); v++) {
				blockStart(dim) = v;
				nextDim(dim - 1);
			}
		}
	};

	// Now start from the last (slowest-varying dimension)
	nextDim(start.rows() - 1);

	#undef DECL_LOOP
	#undef DECL_PTR
}

template<typename IterTp>
void Nifti::readVoxels(const Eigen::Ref<ArrayXs> &start, const Eigen::Ref<ArrayXs> &size, IterTp begin, IterTp end) {
	if (!(m_mode == Mode::Read))
		throw(std::runtime_error("File must be opened for reading: " + basePath()));
	readWriteVoxels(start, size, begin, end);
}

template<typename IterTp>
void Nifti::readVolumes(const size_t first, const size_t nvol, IterTp begin, IterTp end) {
	if (!(m_mode == Mode::Read))
		throw(std::runtime_error("File must be opened for reading: " + basePath()));
	Eigen::Array<size_t, 4, 1> start{0, 0, 0, first};
	Eigen::Array<size_t, 4, 1> size{dim(1), dim(2), dim(3), nvol};
	readWriteVoxels(start, size, begin, end);
}

template<typename IterTp>
void Nifti::writeVoxels(const Eigen::Ref<ArrayXs> &start, const Eigen::Ref<ArrayXs> &size, IterTp begin, IterTp end) {
	if (!(m_mode == Mode::Write))
		throw(std::runtime_error("File must be opened for writing: " + basePath()));
	readWriteVoxels(start, size, begin, end);
}

template<typename IterTp>
void Nifti::writeVolumes(const size_t first, const size_t nvol, IterTp begin, IterTp end) {
	if (!(m_mode == Mode::Write))
		throw(std::runtime_error("File must be opened for writing: " + basePath()));
	Eigen::Array<size_t, 4, 1> start{0, 0, 0, first};
	Eigen::Array<size_t, 4, 1> size{dim(1), dim(2), dim(3), nvol};
	readWriteVoxels(start, size, begin, end);
}

#endif // NIFTI_NIFTI_INL
