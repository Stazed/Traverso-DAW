/*
Copyright (C) 2005-2007 Remon Sijrier 

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

*/


#include "AudioSource.h"
#include "Song.h"
#include "Peak.h"
#include "Export.h"
#include "Utils.h"

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"

// QMutex mutex;


// This constructor is called at file import or recording
AudioSource::AudioSource(const QString& dir, const QString& name)
	: m_dir(dir)
	, m_name(name)
	, m_shortName(name)
	, m_wasRecording (false)
{
	PENTERCONS;
	m_fileName = m_dir + m_name;
	m_id = create_id();
}


// This constructor is called for existing (recorded/imported) audio sources
AudioSource::AudioSource(const QDomNode node)
	: m_dir("")
	, m_name("")
	, m_fileName("")
	, m_wasRecording(false)
{
	set_state(node);
}


AudioSource::~AudioSource()
{
	PENTERDES;
}


QDomNode AudioSource::get_state( QDomDocument doc )
{
	QDomElement node = doc.createElement("Source");
	node.setAttribute("channelcount", m_channelCount);
	node.setAttribute("origsheetid", m_origSongId);
	node.setAttribute("dir", m_dir);
	node.setAttribute("id", m_id);
	node.setAttribute("name", m_name);
	node.setAttribute("origbitdepth", m_origBitDepth);
	node.setAttribute("wasrecording", m_wasRecording);
	node.setAttribute("length", m_length);
	node.setAttribute("rate", m_rate);
	node.setAttribute("decoder", m_decodertype);

	return node;
}


int AudioSource::set_state( const QDomNode & node )
{
	PENTER;
	
	QDomElement e = node.toElement();
	m_channelCount = e.attribute("channelcount", "0").toInt();
	m_origSongId = e.attribute("origsheetid", "0").toLongLong();
	set_dir( e.attribute("dir", "" ));
	m_id = e.attribute("id", "").toLongLong();
	m_length = e.attribute("length", "0").toUInt();
	m_rate = e.attribute("rate", "0").toUInt();
	m_origBitDepth = e.attribute("origbitdepth", "0").toInt();
	m_wasRecording = e.attribute("wasrecording", "0").toInt();
	m_decodertype = e.attribute("decoder", "");
	
	// For older project files, this should properly detect if the 
	// audio source was a recording or not., in fact this should suffice
	// and the flag wasrecording would be unneeded, but oh well....
	if (m_origSongId != 0) {
		m_wasRecording = true;
	}
	
	set_name( e.attribute("name", "No name supplied?" ));
	
	return 1;
}


void AudioSource::set_name(const QString& name)
{
	m_name = name;
	if (m_wasRecording) {
		m_shortName = m_name.left(m_name.length() - 20);
	} else {
		m_shortName = m_name;
	}
	m_fileName = m_dir + m_name;
}


void AudioSource::set_dir(const QString& dir)
{
	m_dir = dir;
	m_fileName = m_dir + m_name;
}


int AudioSource::get_rate( ) const
{
	return m_rate;
}

void AudioSource::set_original_bit_depth( uint bitDepth )
{
	m_origBitDepth = bitDepth;
}

void AudioSource::set_created_by_song(qint64 id)
{
	m_origSongId = id;
}

QString AudioSource::get_filename( ) const
{
	return m_fileName;
}

QString AudioSource::get_dir( ) const
{
	return m_dir;
}

QString AudioSource::get_name( ) const
{
	return m_name;
}

int AudioSource::get_bit_depth( ) const
{
	return m_origBitDepth;
}

QString AudioSource::get_short_name() const
{
	return m_shortName;
}

// eof
