/*
Copyright (C) 2005-2006 Remon Sijrier

This file is part of Traverso

Traverso is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.

$Id: FileHelpers.cpp,v 1.10 2007/11/05 15:49:30 r_sijrier Exp $
*/

#include "FileHelpers.h"

#include <sys/stat.h>
#include <unistd.h>

#include "TConfig.h"
#include "Information.h"
#include <QDir>
#include <Utils.h>
#include <QObject>
#include <QFile>

#include "Debugger.h"

// delete file/dir pName if it is a directory, calls itself recursively
// on any file/dir in the directory before removing the directory
int FileHelper::remove_recursively(const QString& pName)
{
	QString name = config().get_property("Project", "directory", "/directory/unknown").toString();
	name += "/" + pName;

	QFileInfo fileInfo(name);

	if (!fileInfo.exists()) {
        info().warning(tr("File does not exist! %1").arg(name));
		return -1;
	}

	if (!fileInfo.isWritable()) {
        info().warning(tr("failed to remove %s: you don't have write access to it").arg(name));
		return -1;
	}

	if(fileInfo.isFile()) {
		QFile file(name);
		if (!file.remove()) {
            info().warning(tr("failed to remove file %1").arg(name));
			return -1;
		}
		return 1;
	} else if(fileInfo.isDir()) {
		QDir dir(name);
		QFileInfoList list = dir.entryInfoList();
		QFileInfo fi;

		for (int i = 0; i < list.size(); ++i) {
			fi = list.at(i);
			if ((fi.fileName() != ".") && (fi.fileName() != "..")) {
				QString nextFileName = pName + "/" + fi.fileName();
				if (remove_recursively(nextFileName) < 0) {
                    info().warning(tr("failed to remove directory %1").arg(nextFileName));
					return -1;
				}
			}
		}

		if (!dir.rmdir(name)) {
            info().warning(tr("failed to remove directory %1").arg(name));
			return -1;
		}

		return 1;
	}

	return 1;
}


int FileHelper::copy_recursively(const QString& pNameFrom, const QString& pNameTo)
{
#if defined (Q_OS_UNIX) || defined (Q_OS_MAC)
	QString nameFrom = config().get_property("Project", "directory", "/directory/unknown").toString();
	QString nameTo(nameFrom);

	nameFrom += pNameFrom;
	nameTo += pNameTo;

	QFileInfo fileFromInfo(nameFrom);
	QFileInfo fileToInfo(nameTo);

	if (!fileFromInfo.exists()) {
        info().warning(tr("File or directory %1 doesn't exist\n").arg(pNameFrom));
		return -1;
	}
	if (fileToInfo.exists()) {
        info().warning(tr("File or directory %1 already exists").arg(pNameTo));
		return -1;
	}

	if(fileFromInfo.isFile()) {
		QFile fileFrom(nameFrom);
		if (!fileFrom.open(QIODevice::ReadOnly)) {
            info().warning(tr("failed to open file %1 for reading\n").arg(nameFrom));
			return -1;
		}

		QFile fileTo(nameTo);
		if (!fileTo.open(QIODevice::WriteOnly)) {
			fileFrom.close();
            info().warning(tr("failed to open file for writting %1").arg(nameFrom));
			return -1;
		}

		// the real copy part should perhaps be implemented using QDataStream
		// but .handle() will still be needed to get the optimal block-size
		//
		//! \todo does not keep file mode yet
		int bufferSize = 4096;
		int fileDescFrom = fileFrom.handle();
		int fileDescTo = fileTo.handle();

#if defined(DHAVE_SYS_STAT_H)
		struct stat fileStat;
		if (fstat(fileDescFrom, &fileStat) == 0) {
			bufferSize = (int)fileStat.st_blksize;
		}
#endif

		void *buffer = malloc(sizeof(char) * bufferSize);
		// QMemArray<char> buffer(bufferSize);

		for (;;) {
			int nRead = read(fileDescFrom, buffer, bufferSize);
			if (nRead < 0) {
				fileFrom.close();
				fileTo.close();
                info().warning(tr("Error while reading file %1").arg(nameFrom));
				return -1;
			}
			if (nRead == 0)
				break;
			if (write(fileDescTo, buffer, nRead) < 0) {
				fileFrom.close();
				fileTo.close();
                info().warning(tr("Error while writing file %1").arg(nameTo));
				return -1;
			}
		}
		free(buffer);

		fileFrom.close();
		fileTo.close();

		return 0;
	} else if(fileFromInfo.isDir()) {
		QDir dirFrom(nameFrom);
		QDir dirTo(nameTo);
        if (!dirTo.mkdir(nameTo)) {

            info().warning(tr("failed to create directory %1").arg(nameTo));
			return -1;
		}

		QFileInfoList list = dirFrom.entryInfoList();
		QFileInfo fi;
		QString fileName;
		for (int i = 0; i < list.size(); ++i) {
			fileName = fi.fileName();
			if ((fileName != ".") && (fileName != "..")) {
				copy_recursively(pNameFrom + "/" + fileName, pNameTo + "/" + fileName);
			}
		}
		return 0;
	}

#endif

	return -1;
}

QString FileHelper::fileerror_to_string(int error)
{
	switch(error) {
        case QFile::NoError: return QObject::tr("No error occurred");
        case QFile::ReadError: return QObject::tr("An error occurred when reading from the file.");
        case QFile::WriteError: return QObject::tr("An error occurred when writing to the file.");
        case QFile::FatalError: return QObject::tr("A fatal error occurred.");
        case QFile::OpenError: return QObject::tr("The file could not be opened.");
        case QFile::ResourceError: return QObject::tr("Resourc error");
        case QFile::AbortError: return QObject::tr("The operation was aborted.");
        case QFile::TimeOutError: return QObject::tr("A timeout occurred.");
        case QFile::UnspecifiedError: return QObject::tr("An unspecified error occurred.");
        case QFile::RemoveError: return QObject::tr("The file could not be removed.");
        case QFile::RenameError: return QObject::tr("The file could not be renamed.");
        case QFile::PositionError: return QObject::tr("The position in the file could not be changed.");
        case QFile::ResizeError: return QObject::tr("The file could not be resized.");
        case QFile::PermissionsError: return QObject::tr("The file could not be accessed.");
        case QFile::CopyError: return QObject::tr("The file could not be copied.");
		default: return QObject::tr("Unknown error");
	}
}
