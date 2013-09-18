//
//  ZipFile.cpp
//  NiftiImage
//
//  Created by Tobias Wood on 18/09/2013.
//  Copyright (c) 2013 Tobias Wood. All rights reserved.
//

#include "Nifti/ZipFile.h"

//******************************
#pragma mark Methods for ZipFile
//******************************
ZipFile::ZipFile() :
	m_plainFile(nullptr), m_gzipFile(nullptr)
{}

bool ZipFile::open(const string &path, const string &mode, const bool zip) {
	if (m_gzipFile || m_plainFile) {
		close();
	}
	
	if (zip) {
		m_gzipFile = gzopen(path.c_str(), mode.c_str());
	} else {
		m_plainFile = fopen(path.c_str(), mode.c_str());
	}
	
	if (m_gzipFile || m_plainFile) {
		return true;
	} else {
		return false;
	}
}

void ZipFile::close() {
	if (m_gzipFile)
		gzclose(m_gzipFile);
	else if (m_plainFile)
		fclose(m_plainFile);
	m_gzipFile = m_plainFile = NULL;
}

/*! Attempts to read the specified number of bytes into the buffer
 *
 * Currently the best we can do for sizes is unsigned int, as this is what
 * gzread uses
 *
 */
size_t ZipFile::read(void *buff, unsigned size) {
	if (m_gzipFile) {
		unsigned remaining = size, totalRead = 0;
		char *cbuff = (char *)buff;
		while (remaining > 0) {
			unsigned chunkSize = (remaining < numeric_limits<int>::max()) ? remaining : numeric_limits<int>::max();
			int nread = gzread(m_gzipFile, cbuff, static_cast<unsigned int>(chunkSize));
			if (nread <= 0) {
				return 0;
			}
			remaining -= nread;
			if (nread < chunkSize) {
				return totalRead;
			}
			cbuff += nread;
			totalRead += nread;
		}
		return totalRead;
	} else if (m_plainFile) {
		size_t nread = fread(buff, size, 1, m_plainFile) * size;
		if (ferror(m_plainFile)) {
			return 0;
		}
		return nread;
	} else { // Can't read if we don't have a valid file handle open
		return 0;
	}
}

size_t ZipFile::write(const void *buff, int size)
{
	if (buff == nullptr) {
		throw(std::invalid_argument("Attempted to write data from null pointer."));
	}
	if (m_gzipFile) {
		unsigned remaining = size, totalWritten = 0;
		char *chunk = (char *)buff;
		while(remaining > 0 ) {
			unsigned chunkSize = (remaining < numeric_limits<int>::max()) ? remaining : numeric_limits<int>::max();
			int nwritten = gzwrite(m_gzipFile, chunk, chunkSize);
			if (nwritten == 0) {
				return 0;
			}
			remaining -= nwritten;
			if (nwritten < chunkSize) {
				return totalWritten;
			}
			chunk += nwritten;
			totalWritten += nwritten;
		}
		return totalWritten;
	} else if (m_plainFile) {
		size_t nwritten = fwrite(buff, size, 1, m_plainFile) * size;
		if (ferror(m_plainFile)) {
			return 0;
		}
		return nwritten;
	} else { // Can't write anything to a closed file
		return 0;
	}
}

bool ZipFile::seek(long offset, int whence) {
	if (m_gzipFile) {
		long int pos = gzseek(m_gzipFile, offset, whence);
		return (pos != -1);
	} else if (m_plainFile) {
		return (fseek(m_plainFile, offset, whence) == 0);
	} else {
		return false;
	}
}

long ZipFile::tell() const {
	if (m_gzipFile) {
		return gztell(m_gzipFile);
	} else if (m_plainFile) {
		return ftell(m_plainFile);
	} else {
		return 0;
	}
}

void ZipFile::flush() {
	if (m_gzipFile)
		gzflush(m_gzipFile, Z_FINISH);
	else if (m_plainFile)
		fflush(m_plainFile);
}
