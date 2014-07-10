#include "Header.h"
#include "Internal.h"

using namespace std;
using namespace Eigen;

namespace Nifti {

/*
 * Returns the DataType enum for a particular code
 *
 *\sa NIFTI1_TYPE group in nifti1.h
 */
DataType DataTypeForCode(const int code) {
	static const map<int, DataType> c2dt{
		{NIFTI_TYPE_UINT8, DataType::UINT8},
		{NIFTI_TYPE_UINT16, DataType::UINT16},
		{NIFTI_TYPE_UINT32, DataType::UINT32},
		{NIFTI_TYPE_UINT64, DataType::UINT64},
		{NIFTI_TYPE_INT8, DataType::INT8},
		{NIFTI_TYPE_INT16, DataType::INT16},
		{NIFTI_TYPE_INT32, DataType::INT32},
		{NIFTI_TYPE_INT64, DataType::INT64},
		{NIFTI_TYPE_FLOAT32, DataType::FLOAT32},
		{NIFTI_TYPE_FLOAT64, DataType::FLOAT64},
		{NIFTI_TYPE_FLOAT128, DataType::FLOAT128},
		{NIFTI_TYPE_COMPLEX64, DataType::COMPLEX64},
		{NIFTI_TYPE_COMPLEX128, DataType::COMPLEX128},
		{NIFTI_TYPE_COMPLEX256, DataType::COMPLEX256},
		{NIFTI_TYPE_RGB24, DataType::RGB24},
		{NIFTI_TYPE_RGBA32, DataType::RGBA32},
	};
	auto dt = c2dt.find(code);
	if (dt != c2dt.end())
		return dt->second;
	else
		throw(std::invalid_argument("Unsupported data format code: " + to_string(code)));
}


/*	Internal map of datatype properties
 *
 *  The map is declared here because making it a static member of Nifti1 was
 *  causing problems with looking up the datatype in close() when called by
 *  ~Nifti1. It's possible for C++ to destruct static members even when
 *  objects still exist in another translation unit.
 */
const DataTypeInfo &TypeInfo(const DataType dt) {
	static const map<DataType, DataTypeInfo> DTInfo{
		{DataType::UINT8,      {DataType::UINT8,      NIFTI_TYPE_UINT8,       1,  0, "UINT8"} },
		{DataType::UINT16,     {DataType::UINT16,     NIFTI_TYPE_UINT16,      2,  2, "UINT16"} },
		{DataType::UINT32,     {DataType::UINT32,     NIFTI_TYPE_UINT32,      4,  4, "UINT32"} },
		{DataType::UINT64,     {DataType::UINT64,     NIFTI_TYPE_UINT64,      8,  8, "UINT64"} },
		{DataType::INT8,       {DataType::INT8,       NIFTI_TYPE_INT8,        1,  0, "INT8"} },
		{DataType::INT16,      {DataType::INT16,      NIFTI_TYPE_INT16,       2,  2, "INT16"} },
		{DataType::INT32,      {DataType::INT32,      NIFTI_TYPE_INT32,       4,  4, "INT32"} },
		{DataType::INT64,      {DataType::INT64,      NIFTI_TYPE_INT64,       8,  8, "INT64"} },
		{DataType::FLOAT32,    {DataType::FLOAT32,    NIFTI_TYPE_FLOAT32,     4,  4, "FLOAT32"} },
		{DataType::FLOAT64,    {DataType::FLOAT64,    NIFTI_TYPE_FLOAT64,     8,  8, "FLOAT64"} },
		{DataType::FLOAT128,   {DataType::FLOAT128,   NIFTI_TYPE_FLOAT128,   16, 16, "FLOAT128"} },
		{DataType::COMPLEX64,  {DataType::COMPLEX64,  NIFTI_TYPE_COMPLEX64,   8,  4, "COMPLEX64"} },
		{DataType::COMPLEX128, {DataType::COMPLEX128, NIFTI_TYPE_COMPLEX128, 16,  8, "COMPLEX128"} },
		{DataType::COMPLEX256, {DataType::COMPLEX256, NIFTI_TYPE_COMPLEX256, 32, 16, "COMPLEX256"} },
		{DataType::RGB24,      {DataType::RGB24,      NIFTI_TYPE_RGB24,       3,  0, "RGB24"} },
		{DataType::RGBA32,     {DataType::RGBA32,     NIFTI_TYPE_RGBA32,      4,  0, "RGBA32"} }
	};
	auto info = DTInfo.find(dt);
	if (info != DTInfo.end())
		return info->second;
	else
		throw(std::invalid_argument("Missing type information, contact libNifti1 author."));
}

/*
 * Returns the string representation of a NIfTI transform code.
 *
 *\sa NIFTI1_XFORM_CODES group in nifti1.h
 */
const string XFormName(const XForm c) {
	switch (c) {
		case XForm::Unknown: return "Unknown"; break;
		case XForm::ScannerAnatomy: return "Scanner Anatomy"; break;
		case XForm::AlignedAnatomy: return "Aligned Anatomy"; break;
		case XForm::Talairach: return "Talairach"; break;
		case XForm::MNI_152: return "MNI 152"; break;
	}
}

/*
 * Converts a NIfTI transform enum into the corresponding integer code
 *
 *\sa NIFTI1_XFORM_CODES group in nifti1.h
 */
int XFormCode(const XForm c) {
	switch (c) {
		case XForm::Unknown: return NIFTI_XFORM_UNKNOWN; break;
		case XForm::ScannerAnatomy: return NIFTI_XFORM_SCANNER_ANAT; break;
		case XForm::AlignedAnatomy: return NIFTI_XFORM_ALIGNED_ANAT; break;
		case XForm::Talairach: return NIFTI_XFORM_TALAIRACH; break;
		case XForm::MNI_152: return NIFTI_XFORM_MNI_152; break;
	}
}

/*
 * Returns an integer code into the corresponding NIfTI transform enum
 *
 *\sa NIFTI1_XFORM_CODES group in nifti1.h
 */
XForm XFormForCode(const int c) {
	switch (c) {
		case NIFTI_XFORM_UNKNOWN: return XForm::Unknown; break;
		case NIFTI_XFORM_SCANNER_ANAT: return XForm::ScannerAnatomy; break;
		case NIFTI_XFORM_ALIGNED_ANAT: return XForm::AlignedAnatomy; break;
		case NIFTI_XFORM_TALAIRACH: return XForm::Talairach; break;
		case NIFTI_XFORM_MNI_152: return XForm::MNI_152; break;
		default:
			throw(std::invalid_argument("Invalid transform code: " + to_string(c)));
	}
}

Header::Header() :
	scaling_slope(1.), scaling_inter(0.), calibration_min(0.), calibration_max(0.),
	freq_dim(0), phase_dim(0), slice_dim(0),
	slice_code(0), slice_start(0), slice_end(0), slice_duration(0),
	toffset(0), xyz_units(NIFTI_UNITS_MM), time_units(NIFTI_UNITS_SEC),
	intent_code(NIFTI_INTENT_NONE), intent_p1(0), intent_p2(0), intent_p3(0),
	intent_name(""), description(""), aux_file(""),
	m_qform(Affine3f::Identity()), m_sform(Affine3d::Identity())
{}

Header::Header(const struct nifti_1_header &nhdr) {
	m_typeinfo = TypeInfo(DataTypeForCode(nhdr.datatype));

	if((nhdr.dim[0] < 1) || (nhdr.dim[0] > 7)) {
		throw(std::runtime_error("Invalid rank in header struct."));
	}
	for (int i = 0; i < nhdr.dim[0]; i++) {
		m_dim[i] = nhdr.dim[i + 1];
		m_voxdim[i] = nhdr.pixdim[i + 1];
	}
	for (int i = nhdr.dim[0]; i < 7; i++) {
		m_dim[i] = 1;
		m_voxdim[i] = 1.;
	}
	calcStrides();
	// Compute Q-Form
	Affine3f S; S = Scaling(m_voxdim[0], m_voxdim[1], m_voxdim[2]);
	if(nhdr.qform_code <= 0) {
		// If Q-Form not set
		m_qform = S.matrix();
		m_qcode = XForm::Unknown;
	} else {
		float b = FixFloat(nhdr.quatern_b);
		float c = FixFloat(nhdr.quatern_c);
		float d = FixFloat(nhdr.quatern_d);
		float a = sqrt(1 - (b*b + c*c + d*d));

		float x = FixFloat(nhdr.qoffset_x);
		float y = FixFloat(nhdr.qoffset_y);
		float z = FixFloat(nhdr.qoffset_z);

		Quaternionf Q(a, b, c, d);
		Affine3f T; T = Translation3f(x, y, z);
		m_qform = T*Q*S;

		// Fix left-handed co-ords in a very dumb way (see writeHeader())
		if (nhdr.pixdim[0] < 0.)
			m_qform.matrix().block(0, 2, 3, 1) *= -1.;
		m_qcode = XFormForCode(nhdr.qform_code);
	}
	// Load S-Form
	if(nhdr.sform_code <= 0) {
		m_sform = S.matrix();
		m_scode = XForm::Unknown;
	} else {
		m_sform.setIdentity();
		for (int i = 0; i < 4; i++) {
			m_sform(0, i) = nhdr.srow_x[i];
			m_sform(1, i) = nhdr.srow_y[i];
			m_sform(2, i) = nhdr.srow_z[i];
		}
		m_scode = XFormForCode(nhdr.sform_code);
	}

	scaling_slope = FixFloat(nhdr.scl_slope);
	scaling_inter = FixFloat(nhdr.scl_inter);
	if (scaling_slope == 0.)
		scaling_slope = 1.;

	intent_code     = nhdr.intent_code;
	intent_p1       = FixFloat(nhdr.intent_p1);
	intent_p2       = FixFloat(nhdr.intent_p2);
	intent_p3       = FixFloat(nhdr.intent_p3);
	toffset         = FixFloat(nhdr.toffset);

	intent_name     = string(nhdr.intent_name);
	xyz_units       = XYZT_TO_SPACE(nhdr.xyzt_units);
	time_units      = XYZT_TO_TIME(nhdr.xyzt_units);
	freq_dim        = DIM_INFO_TO_FREQ_DIM (nhdr.dim_info);
	phase_dim       = DIM_INFO_TO_PHASE_DIM(nhdr.dim_info);
	slice_dim       = DIM_INFO_TO_SLICE_DIM(nhdr.dim_info);
	slice_code      = nhdr.slice_code;
	slice_start     = nhdr.slice_start;
	slice_end       = nhdr.slice_end;
	slice_duration  = FixFloat(nhdr.slice_duration);
	calibration_min = FixFloat(nhdr.cal_min);
	calibration_max = FixFloat(nhdr.cal_max);

	description     = string(nhdr.descrip);
	aux_file        = string(nhdr.aux_file);

	m_voxoffset = (int)nhdr.vox_offset;
	if (m_voxoffset < (int)sizeof(nhdr))
		m_voxoffset = (int)sizeof(nhdr);
}

Header::Header(const struct nifti_2_header &nhdr) {
	m_typeinfo = TypeInfo(DataTypeForCode(nhdr.datatype));

	if((nhdr.dim[0] < 1) || (nhdr.dim[0] > 7)) {
		throw(std::runtime_error("Invalid rank in header struct."));
	}
	for (int i = 0; i < nhdr.dim[0]; i++) {
		m_dim[i] = nhdr.dim[i + 1];
		m_voxdim[i] = nhdr.pixdim[i + 1];
	}
	for (int i = nhdr.dim[0]; i < 7; i++) {
		m_dim[i] = 1;
		m_voxdim[i] = 1.;
	}
	calcStrides();
	// Compute Q-Form
	Affine3f S; S = Scaling(m_voxdim[0], m_voxdim[1], m_voxdim[2]);
	if(nhdr.qform_code <= 0) {
		// If Q-Form not set
		m_qform = S.matrix();
		m_qcode = XForm::Unknown;
	} else {
		float b = FixFloat(nhdr.quatern_b);
		float c = FixFloat(nhdr.quatern_c);
		float d = FixFloat(nhdr.quatern_d);
		float a = sqrt(1 - (b*b + c*c + d*d));

		float x = FixFloat(nhdr.qoffset_x);
		float y = FixFloat(nhdr.qoffset_y);
		float z = FixFloat(nhdr.qoffset_z);

		Quaternionf Q(a, b, c, d);
		Affine3f T; T = Translation3f(x, y, z);
		m_qform = T*Q*S;

		// Fix left-handed co-ords in a very dumb way (see writeHeader())
		if (nhdr.pixdim[0] < 0.)
			m_qform.matrix().block(0, 2, 3, 1) *= -1.;
		m_qcode = XFormForCode(nhdr.qform_code);
	}
	// Load S-Form
	if(nhdr.sform_code <= 0) {
		m_sform = S.matrix();
		m_scode = XForm::Unknown;
	} else {
		m_sform.setIdentity();
		for (int i = 0; i < 4; i++) {
			m_sform(0, i) = nhdr.srow_x[i];
			m_sform(1, i) = nhdr.srow_y[i];
			m_sform(2, i) = nhdr.srow_z[i];
		}
		m_scode = XFormForCode(nhdr.sform_code);
	}

	scaling_slope = FixFloat(nhdr.scl_slope);
	scaling_inter = FixFloat(nhdr.scl_inter);
	if (scaling_slope == 0.)
		scaling_slope = 1.;

	intent_code     = nhdr.intent_code;
	intent_p1       = FixFloat(nhdr.intent_p1);
	intent_p2       = FixFloat(nhdr.intent_p2);
	intent_p3       = FixFloat(nhdr.intent_p3);
	toffset         = FixFloat(nhdr.toffset);

	intent_name     = string(nhdr.intent_name);
	xyz_units       = XYZT_TO_SPACE(nhdr.xyzt_units);
	time_units      = XYZT_TO_TIME(nhdr.xyzt_units);
	freq_dim        = DIM_INFO_TO_FREQ_DIM (nhdr.dim_info);
	phase_dim       = DIM_INFO_TO_PHASE_DIM(nhdr.dim_info);
	slice_dim       = DIM_INFO_TO_SLICE_DIM(nhdr.dim_info);
	slice_code      = nhdr.slice_code;
	slice_start     = nhdr.slice_start;
	slice_end       = nhdr.slice_end;
	slice_duration  = FixFloat(nhdr.slice_duration);
	calibration_min = FixFloat(nhdr.cal_min);
	calibration_max = FixFloat(nhdr.cal_max);

	description     = string(nhdr.descrip);
	aux_file        = string(nhdr.aux_file);

	m_voxoffset = (int)nhdr.vox_offset;
	if (m_voxoffset < (int)sizeof(nhdr))
		m_voxoffset = (int)sizeof(nhdr);
}

Header::Header(const int nx, const int ny, const int nz, const int nt,
		     const float dx, const float dy, const float dz, const float dt,
			 const DataType dtype) :
	Header()
{
	m_typeinfo = TypeInfo(dtype);
	m_dim[0] = nx < 1 ? 1 : nx;
	m_dim[1] = ny < 1 ? 1 : ny;
	m_dim[2] = nz < 1 ? 1 : nz;
	m_dim[3] = nt < 1 ? 1 : nt;
	m_voxdim[0] = dx; m_voxdim[1] = dy; m_voxdim[2] = dz; m_voxdim[3] = dt;
	Affine3f S; S = Scaling(m_voxdim[0], m_voxdim[1], m_voxdim[2]);
	setTransform(S);
	calcStrides();
}

Header::Header(const IndexArray &dim, const ArrayXf &voxdim, const DataType dtype) :
	Header()
{
	assert(dim.rows() < 8);
	assert(dim.rows() == voxdim.rows());

	m_dim.head(dim.rows()) = dim;
	m_voxdim.head(voxdim.rows()) = voxdim;
	m_typeinfo = TypeInfo(dtype);
	Affine3f S; S = Scaling(m_voxdim[0], m_voxdim[1], m_voxdim[2]);
	setTransform(S);
	calcStrides();
}

Header::operator nifti_1_header() const {
	struct nifti_1_header nhdr;
	memset(&nhdr,0,sizeof(nhdr));
	nhdr.sizeof_hdr = sizeof(nhdr);
	nhdr.regular    = 'r'; // For unknown reasons

	nhdr.dim[0] = rank(); //pixdim[0] is set later with qform
	for (size_t i = 0; i < 7; i++) { // Convert back to short/float
		if (m_dim[i] > numeric_limits<short>::max()) {
			throw(std::runtime_error("Nifti1 does not support dimensions greater than " + to_string(numeric_limits<short>::max())));
		}
		nhdr.dim[i + 1] = m_dim[i];
		nhdr.pixdim[i + 1] = m_voxdim[i];
	}

	nhdr.datatype = m_typeinfo.code;
	nhdr.bitpix   = 8 * m_typeinfo.size;

	if(calibration_max > calibration_min) {
		nhdr.cal_max = calibration_max;
		nhdr.cal_min = calibration_min;
	}

	if(scaling_slope != 0.0) {
		nhdr.scl_slope = scaling_slope;
		nhdr.scl_inter = scaling_inter;
	}

	strncpy(nhdr.descrip, description.c_str(), 80);
	strncpy(nhdr.aux_file, aux_file.c_str(), 24);

	strcpy(nhdr.magic,m_magic.c_str());

	nhdr.intent_code = intent_code;
	nhdr.intent_p1   = intent_p1;
	nhdr.intent_p2   = intent_p2;
	nhdr.intent_p3   = intent_p3;
	strncpy(nhdr.intent_name, intent_name.c_str(), 16);

	// Check that m_voxoffset is sensible
	/*if (m_nii)
		m_voxoffset = sizeof(nhdr) + 4 + totalExtensionSize(); // The 4 is for the nifti_extender struct
	else
		m_voxoffset = 0;*/

	nhdr.vox_offset = m_voxoffset;
	nhdr.xyzt_units = SPACE_TIME_TO_XYZT(xyz_units, time_units);
	nhdr.toffset    = toffset ;

	nhdr.qform_code = XFormCode(m_qcode);
	Quaternionf Q(m_qform.rotation());
	Translation3f T(m_qform.translation());
	// Fix left-handed co-ord systems in an incredibly dumb manner.
	// First - NIFTI stores this information in pixdim[0], with both inconsistent
	// documentation and a reference implementation that hides pixdim[0] on reading
	// Second - Eigen .rotation() simultaneously calculates a scaling, and so may
	// hide axes flips. Hence we need to use .linear() to get the determinant
	if (m_qform.linear().determinant() < 0)
		nhdr.pixdim[0] = -1.;
	else
		nhdr.pixdim[0] = 1.;
	// NIfTI REQUIRES a (or w) >= 0. Because Q and -Q represent the same
	// rotation, if w < 0 simply store -Q
	if (Q.w() < 0) {
		nhdr.quatern_b  = -Q.x();
		nhdr.quatern_c  = -Q.y();
		nhdr.quatern_d  = -Q.z();
	} else {
		nhdr.quatern_b = Q.x();
		nhdr.quatern_c = Q.y();
		nhdr.quatern_d = Q.z();
	}
	nhdr.qoffset_x  = T.x();
	nhdr.qoffset_y  = T.y();
	nhdr.qoffset_z  = T.z();

	nhdr.sform_code = XFormCode(m_scode);
	for (int i = 0; i < 4; i++) {
		nhdr.srow_x[i]  = m_sform(0, i);
		nhdr.srow_y[i]  = m_sform(1, i);
		nhdr.srow_z[i]  = m_sform(2, i);
	}

	nhdr.dim_info = FPS_INTO_DIM_INFO(freq_dim, phase_dim, slice_dim);
	nhdr.slice_code     = slice_code;
	nhdr.slice_start    = slice_start;
	nhdr.slice_end      = slice_end;
	nhdr.slice_duration = slice_duration;

	return nhdr;
}

Header::operator nifti_2_header() const {
	struct nifti_2_header nhdr;
	memset(&nhdr,0,sizeof(nhdr));
	nhdr.sizeof_hdr = sizeof(nhdr);

	nhdr.dim[0] = rank(); //pixdim[0] is set later with qform
	for (size_t i = 0; i < 7; i++) { // Convert back to short/float
		if (m_dim[i] > numeric_limits<short>::max()) {
			throw(std::runtime_error("Nifti1 does not support dimensions greater than " + to_string(numeric_limits<short>::max())));
		}
		nhdr.dim[i + 1] = m_dim[i];
		nhdr.pixdim[i + 1] = m_voxdim[i];
	}

	nhdr.datatype = m_typeinfo.code;
	nhdr.bitpix   = 8 * m_typeinfo.size;

	if(calibration_max > calibration_min) {
		nhdr.cal_max = calibration_max;
		nhdr.cal_min = calibration_min;
	}

	if(scaling_slope != 0.0) {
		nhdr.scl_slope = scaling_slope;
		nhdr.scl_inter = scaling_inter;
	}

	strncpy(nhdr.descrip, description.c_str(), 80);
	strncpy(nhdr.aux_file, aux_file.c_str(), 24);

	strcpy(nhdr.magic,m_magic.c_str());

	nhdr.intent_code = intent_code;
	nhdr.intent_p1   = intent_p1;
	nhdr.intent_p2   = intent_p2;
	nhdr.intent_p3   = intent_p3;
	strncpy(nhdr.intent_name, intent_name.c_str(), 16);

	// Check that m_voxoffset is sensible
	/*if (m_nii)
		m_voxoffset = sizeof(nhdr) + 4 + totalExtensionSize(); // The 4 is for the nifti_extender struct
	else
		m_voxoffset = 0;*/

	nhdr.vox_offset = m_voxoffset;
	nhdr.xyzt_units = SPACE_TIME_TO_XYZT(xyz_units, time_units);
	nhdr.toffset    = toffset ;

	nhdr.qform_code = XFormCode(m_qcode);
	Quaternionf Q(m_qform.rotation());
	Translation3f T(m_qform.translation());
	// Fix left-handed co-ord systems in an incredibly dumb manner.
	// First - NIFTI stores this information in pixdim[0], with both inconsistent
	// documentation and a reference implementation that hides pixdim[0] on reading
	// Second - Eigen .rotation() simultaneously calculates a scaling, and so may
	// hide axes flips. Hence we need to use .linear() to get the determinant
	if (m_qform.linear().determinant() < 0)
		nhdr.pixdim[0] = -1.;
	else
		nhdr.pixdim[0] = 1.;
	// NIfTI REQUIRES a (or w) >= 0. Because Q and -Q represent the same
	// rotation, if w < 0 simply store -Q
	if (Q.w() < 0) {
		nhdr.quatern_b  = -Q.x();
		nhdr.quatern_c  = -Q.y();
		nhdr.quatern_d  = -Q.z();
	} else {
		nhdr.quatern_b = Q.x();
		nhdr.quatern_c = Q.y();
		nhdr.quatern_d = Q.z();
	}
	nhdr.qoffset_x  = T.x();
	nhdr.qoffset_y  = T.y();
	nhdr.qoffset_z  = T.z();

	nhdr.sform_code = XFormCode(m_scode);
	for (int i = 0; i < 4; i++) {
		nhdr.srow_x[i]  = m_sform(0, i);
		nhdr.srow_y[i]  = m_sform(1, i);
		nhdr.srow_z[i]  = m_sform(2, i);
	}

	nhdr.dim_info = FPS_INTO_DIM_INFO(freq_dim, phase_dim, slice_dim);
	nhdr.slice_code     = slice_code;
	nhdr.slice_start    = slice_start;
	nhdr.slice_end      = slice_end;
	nhdr.slice_duration = slice_duration;

	return nhdr;
}

/**
  *   Simple function to calculate the strides into the data on disk. Used for
  *   subvolume/voxel-wise reads.
  */
void Header::calcStrides() {
	m_strides = Array<size_t, 7, 1>::Ones();
	for (size_t i = 1; i < rank(); i++) {
		m_strides(i) = m_strides(i - 1) * m_dim(i - 1);
	}
}

const DataTypeInfo & Header::typeInfo() const { return m_typeinfo; }
DataType Header::datatype() const { return m_typeinfo.type; }
void Header::setDatatype(const DataType dt) { m_typeinfo = TypeInfo(dt);
}

const string &Header::magic() const { return m_magic; }
void Header::setMagic(const Version v, const bool isNii) {
	if (v == Version::Nifti1) {
		if (isNii)
			m_magic = "n+1";
		else
			m_magic = "ni1";
	} else {
		if (isNii)
			m_magic = "n+2";
		else
			m_magic = "ni2";
	}
}

Index Header::rank() const {
	for (size_t d = m_voxdim.rows(); d > 0; d--) {
		if (m_dim[d - 1] > 1) {
			return d;
		}
	}
	return 1;
}

Index Header::dim(const Index d) const {
	assert((d > 0) && (d <= m_dim.rows()));
	return m_dim[d - 1];
}
void Header::setDim(const Index d, const Index size) {
	assert((d > 0) && (d <= m_dim.rows()));
	m_dim[d - 1] = size;
}

IndexArray Header::dims() const { return m_dim.head(rank()); }
IndexArray Header::strides() const { return m_strides.head(rank()); }
Index Header::voxoffset() const { return m_voxoffset; }

float Header::voxDim(const size_t d) const {
	assert((d > 0) && (d <= m_voxdim.rows()));
	return m_voxdim[d - 1];
}
void Header::setVoxDim(const size_t d, const float f) {
	assert((d > 0) && (d <= m_voxdim.rows()));
	m_voxdim[d] = f;
}
ArrayXf Header::voxDims() const { return m_voxdim.head(rank()); }
void Header::setVoxDims(const ArrayXf &n) {
	assert(n.rows() <= m_voxdim.rows());
	m_voxdim.head(n.rows()) = n;
}

const Affine3f &Header::qform() const { return m_qform; }
const Affine3f &Header::sform() const { return m_sform; }
const XForm &Header::qcode() const { return m_qcode; }
const XForm &Header::scode() const { return m_scode; }
const Affine3f &Header::transform() const {
	if ((m_scode > XForm::Unknown) && (m_scode >= m_qcode))
		return m_sform;
	else // There is always a m_qform matrix
		return m_qform;
}

void Header::setTransform(const Affine3f &t, const XForm tc) {
	m_qform = t;
	m_sform = t;
	m_qcode = tc;
	m_scode = tc;
}

bool Header::matchesVoxels(const Header &other) const {
	// Only check the first 3 dimensions
	if ((m_dim.head(3) == other.m_dim.head(3)).all() && (m_voxdim.head(3).isApprox(other.m_voxdim.head(3))))
		return true;
	else
		return false;
}

bool Header::matchesSpace(const Header &other) const {
	if (matchesVoxels(other) && transform().isApprox(other.transform()))
		return true;
	else
		return false;
}

/*
 * Returns the string representation of a NIfTI space unit code.
 *
 *\sa NIFTI1_UNITS group in nifti1.h
 */
const string &Header::spaceUnits() const {
	static const map<int, string> Units {
		{ NIFTI_UNITS_METER,  "m" },
		{ NIFTI_UNITS_MM,     "mm" },
		{ NIFTI_UNITS_MICRON, "um" }
	};
	static const string unknown("Unknown space units code");
	auto it = Units.find(xyz_units);
	if (it == Units.end())
		return unknown;
	else
		return it->second;
}
/*
 * Returns the string description of a NIfTI time unit code.
 *
 *\sa NIFTI1_UNITS group in nifti1.h
 */
const string &Header::timeUnits() const {
	static const map<int, string> Units {
		{ NIFTI_UNITS_SEC,    "s" },
		{ NIFTI_UNITS_MSEC,   "ms" },
		{ NIFTI_UNITS_USEC,   "us" },
		{ NIFTI_UNITS_HZ,     "Hz" },
		{ NIFTI_UNITS_PPM,    "ppm" },
		{ NIFTI_UNITS_RADS,   "rad/s" }
	};
	static const string unknown("Unknown time units code");
	auto it = Units.find(time_units);
	if (it == Units.end())
		return unknown;
	else
		return it->second;
}

/*
 * Returns the string representation of NIfTI intent code.
 *
 *\sa NIFTI1_INTENT_CODES group in nifti1.h
 */
const string &Header::intentName() const {
	static const map<int, string> Intents {
		{ NIFTI_INTENT_CORREL,     "Correlation statistic" },
		{ NIFTI_INTENT_TTEST,      "T-statistic" },
		{ NIFTI_INTENT_FTEST,      "F-statistic" },
		{ NIFTI_INTENT_ZSCORE,     "Z-score"     },
		{ NIFTI_INTENT_CHISQ,      "Chi-squared distribution" },
		{ NIFTI_INTENT_BETA,       "Beta distribution" },
		{ NIFTI_INTENT_BINOM,      "Binomial distribution" },
		{ NIFTI_INTENT_GAMMA,      "Gamma distribution" },
		{ NIFTI_INTENT_POISSON,    "Poisson distribution" },
		{ NIFTI_INTENT_NORMAL,     "Normal distribution" },
		{ NIFTI_INTENT_FTEST_NONC, "F-statistic noncentral" },
		{ NIFTI_INTENT_CHISQ_NONC, "Chi-squared noncentral" },
		{ NIFTI_INTENT_LOGISTIC,   "Logistic distribution" },
		{ NIFTI_INTENT_LAPLACE,    "Laplace distribution" },
		{ NIFTI_INTENT_UNIFORM,    "Uniform distribition" },
		{ NIFTI_INTENT_TTEST_NONC, "T-statistic noncentral" },
		{ NIFTI_INTENT_WEIBULL,    "Weibull distribution" },
		{ NIFTI_INTENT_CHI,        "Chi distribution" },
		{ NIFTI_INTENT_INVGAUSS,   "Inverse Gaussian distribution" },
		{ NIFTI_INTENT_EXTVAL,     "Extreme Value distribution" },
		{ NIFTI_INTENT_PVAL,       "P-value" },
		{ NIFTI_INTENT_LOGPVAL,    "Log P-value" },
		{ NIFTI_INTENT_LOG10PVAL,  "Log10 P-value" },
		{ NIFTI_INTENT_ESTIMATE,   "Estimate" },
		{ NIFTI_INTENT_LABEL,      "Label index" },
		{ NIFTI_INTENT_NEURONAME,  "NeuroNames index" },
		{ NIFTI_INTENT_GENMATRIX,  "General matrix" },
		{ NIFTI_INTENT_SYMMATRIX,  "Symmetric matrix" },
		{ NIFTI_INTENT_DISPVECT,   "Displacement vector" },
		{ NIFTI_INTENT_VECTOR,     "Vector" },
		{ NIFTI_INTENT_POINTSET,   "Pointset" },
		{ NIFTI_INTENT_TRIANGLE,   "Triangle" },
		{ NIFTI_INTENT_QUATERNION, "Quaternion" },
		{ NIFTI_INTENT_DIMLESS,    "Dimensionless number" }
	};
	static const string unknown("Unknown intent code");
	auto it = Intents.find(intent_code);
	if (it == Intents.end())
		return unknown;
	else
		return it->second;
}

/*
 * Returns the string representation of a NIfTI slice_code
 *
 *\sa NIFTI1_SLICE_ORDER group in nifti1.h
 */
const string &Header::sliceName() const {
	static const map<int, string> SliceOrders {
		{ NIFTI_SLICE_SEQ_INC,  "sequential_increasing"    },
		{ NIFTI_SLICE_SEQ_DEC,  "sequential_decreasing"    },
		{ NIFTI_SLICE_ALT_INC,  "alternating_increasing"   },
		{ NIFTI_SLICE_ALT_DEC,  "alternating_decreasing"   },
		{ NIFTI_SLICE_ALT_INC2, "alternating_increasing_2" },
		{ NIFTI_SLICE_ALT_DEC2, "alternating_decreasing_2" }
	};
	static const string unknown("Unknown slice order code");
	auto it = SliceOrders.find(slice_code);
	if (it == SliceOrders.end())
		return unknown;
	else
		return it->second;
}
} // End namespace Nifti
