/*Copyright (C) 2006-2009 Remon Sijrier

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



#include "LV2Plugin.h"

#include <PluginManager.h>
#include <AudioBus.h>
#include <AudioDevice.h>
#include <Utils.h>

#if defined Q_OS_MAC
	#include <cmath>
	extern "C" int isnan(double);
#endif
        
// URI map/unmap features.
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

static QHash<QString, LV2_URID>    g_uri_map;
static QHash<LV2_URID, QByteArray> g_ids_map;

static LV2_URID traverso_lv2_urid_map (
	LV2_URID_Map_Handle /*handle*/, const char *uri )
{
    return LV2Plugin::lv2_urid_map(uri);
}

static LV2_URID_Map g_lv2_urid_map =
	{ NULL, traverso_lv2_urid_map };
static const LV2_Feature g_lv2_urid_map_feature =
	{ LV2_URID_MAP_URI, &g_lv2_urid_map };


static const char *traverso_lv2_urid_unmap (
	LV2_URID_Unmap_Handle /*handle*/, LV2_URID id )
{
    return LV2Plugin::lv2_urid_unmap(id);
}

static LV2_URID_Unmap g_lv2_urid_unmap =
    { NULL, traverso_lv2_urid_unmap };
static const LV2_Feature g_lv2_urid_unmap_feature =
    { LV2_URID_UNMAP_URI, &g_lv2_urid_unmap };


// URI map helpers (static).
LV2_URID LV2Plugin::lv2_urid_map ( const char *uri )
{
	const QString sUri(uri);

	QHash<QString, uint32_t>::ConstIterator iter
		= g_uri_map.constFind(sUri);
	if (iter == g_uri_map.constEnd()) {
		LV2_URID id = g_uri_map.size() + 1000;
		g_uri_map.insert(sUri, id);
		g_ids_map.insert(id, sUri.toUtf8());
		return id;
	}

	return iter.value();
}

const char *LV2Plugin::lv2_urid_unmap ( LV2_URID id )
{
	QHash<LV2_URID, QByteArray>::ConstIterator iter
		= g_ids_map.constFind(id);
	if (iter == g_ids_map.constEnd())
		return NULL;

	return iter.value().constData();
}

// LV2 Presets: port value setter.
static void traverso_lv2_set_port_value ( const char *port_symbol,
	void *user_data, const void *value, uint32_t size, uint32_t type )
{
    LV2Plugin *pLv2Plugin = static_cast<LV2Plugin *> (user_data);
    if (pLv2Plugin == NULL)
            return;

    const LilvPlugin *plugin = pLv2Plugin->get_slv2_plugin();
    if (plugin == NULL)
            return;

    if (size != sizeof(float))
            return;

    LilvWorld* world = PluginManager::instance()->get_lilv_world();

    LilvNode *symbol = lilv_new_string(world, port_symbol);

    const LilvPort *port = lilv_plugin_get_port_by_symbol(plugin, symbol);

    if (port)
    {
        const float val = *(float *) value;
        const unsigned long port_index = lilv_port_get_index(plugin, port);

        pLv2Plugin->update_parameter_value(port_index, val);
    }

    lilv_node_free(symbol);
}


#include <Debugger.h>

#define UC_(x) (const unsigned char* ) x.toLatin1().data()

enum PortDirection {
	INPUT,
	OUTPUT
};

enum PortType {
	CONTROL,
	AUDIO,
        EVENT,
        UNKNOWN
};


LV2Plugin::LV2Plugin(TSession* session, bool slave)
        : Plugin(session)
    , m_plugin(nullptr)
    ,m_input_class(NULL)
    ,m_output_class(NULL)
    ,m_control_class(NULL)
    ,m_audio_class(NULL)
    ,m_event_class(NULL)
{
    m_isSlave = slave;
    m_isLV2 = true;
}


LV2Plugin::LV2Plugin(TSession* session, char* pluginUri)
        : Plugin(session)
    , m_pluginUri(pluginUri)
    , m_plugin(nullptr)
    ,m_input_class(NULL)
    ,m_output_class(NULL)
    ,m_control_class(NULL)
    ,m_audio_class(NULL)
    ,m_event_class(NULL)
    , m_isSlave(false)
{
    m_isLV2 = true;
}


LV2Plugin::~LV2Plugin()
{
	/* Deactivate and free plugin instance */
	if (m_instance) {
		lilv_instance_deactivate(m_instance);
		lilv_instance_free(m_instance);
	}
        
        lilv_node_free(m_input_class);
        lilv_node_free(m_output_class);
        lilv_node_free(m_control_class);
        lilv_node_free(m_audio_class);
        lilv_node_free(m_event_class);
        
        if(m_slave != nullptr)
            delete(m_slave);
}


QDomNode LV2Plugin::get_state( QDomDocument doc )
{
	QDomElement node = Plugin::get_state(doc).toElement();
	
	node.setAttribute("type", "LV2Plugin");
	node.setAttribute("uri", m_pluginUri);
	
	return node;
}


int LV2Plugin::set_state(const QDomNode & node )
{
	Plugin::set_state(node);
	
	QDomElement e = node.toElement();
	
	m_pluginUri = e.attribute( "uri", "");
	
	if (create_instance() < 0) {
		return -1;
	}
	
	/* Create control ports */
	QDomElement controlPortsNode = node.firstChildElement("ControlPorts");
	if (!controlPortsNode.isNull()) {
		QDomNode portNode = controlPortsNode.firstChild();
		
		while (!portNode.isNull()) {
			
			LV2ControlPort* port = new LV2ControlPort(this, portNode);
			m_controlPorts.append(port);
			
			portNode = portNode.nextSibling();
		}
	}
		
	// Create audio input ports 	
	QDomElement audioInputPortsNode = node.firstChildElement("AudioInputPorts");
	if (!audioInputPortsNode.isNull()) {
		QDomNode portNode = audioInputPortsNode.firstChild();
		
		while (!portNode.isNull()) {
			AudioInputPort* port = new AudioInputPort(this);
			port->set_state(portNode);
			m_audioInputPorts.append(port);
			
			portNode = portNode.nextSibling();
		}
	}
	
	// Create audio ouput ports
	QDomElement audioOutputPortsNode = node.firstChildElement("AudioOutputPorts");
	if (!audioOutputPortsNode.isNull()) {
		QDomNode portNode = audioOutputPortsNode.firstChild();
		
		while (!portNode.isNull()) {
			AudioOutputPort* port = new AudioOutputPort(this);
			port->set_state(portNode);
			m_audioOutputPorts.append(port);
			
			portNode = portNode.nextSibling();
		}
	}
        
        get_plugin_presets();
        
	/* Activate the plugin instance */
	lilv_instance_activate(m_instance);
	
	// The default is 2 channels in - out, if there is only 1 in - out, duplicate
	// this plugin, and use it on the second channel
	if (!m_isSlave && m_audioInputPorts.size() == 1 && m_audioOutputPorts.size() == 1) {
		m_slave = create_copy();
	}
	
	return 1;
}


int LV2Plugin::init()
{
	m_num_ports = 0;
        m_ports = nullptr;
	
	LilvWorld* world = PluginManager::instance()->get_lilv_world();
        
	/* Set up the port classes this app supports */
	m_input_class = lilv_new_uri(world, LILV_URI_INPUT_PORT);
	m_output_class = lilv_new_uri(world, LILV_URI_OUTPUT_PORT);
	m_control_class = lilv_new_uri(world, LILV_URI_CONTROL_PORT);
	m_audio_class = lilv_new_uri(world, LILV_URI_AUDIO_PORT);
	m_event_class = lilv_new_uri(world, LILV_URI_EVENT_PORT);   // never used  

	if (create_instance() < 0) {
		return -1;
	}

        get_plugin_presets();
        
	/* Create ports */
	m_num_ports  = lilv_plugin_get_num_ports(m_plugin);
// 	float* default_values  = new float(slv2_plugin_get_num_ports(m_plugin) * sizeof(float));
    float* default_values  = static_cast<float*>(calloc(lilv_plugin_get_num_ports(m_plugin),
                    sizeof(float)));
    lilv_plugin_get_port_ranges_float(m_plugin, nullptr, nullptr, default_values);


    for (uint32_t i=0; i < m_num_ports; ++i)
    {
        LV2ControlPort* port = create_port(i, default_values[i]);
        if (port)
        {
                m_controlPorts.append(port);
        } else
        {
                // Not an audio port, or unrecognized port type
        }
    }
	
	free(default_values);
        
	/* Activate the plugin instance */
	lilv_instance_activate(m_instance);
	
	if (m_audioInputPorts.size() == 0) {
//		PERROR("Plugin %s has no audio input ports set!!", QS_C(get_name()));
		return -1;
	}

	if (m_audioOutputPorts.size() == 0) {
//		PERROR("Plugin %s has no audio output ports set!!", QS_C(get_name()));
		return -1;
	}
	
	// The default is 2 channels in - out, if there is only 1 in - out, duplicate
	// this plugin, and use it on the second channel
	if (!m_isSlave && m_audioInputPorts.size() == 1 && m_audioOutputPorts.size() == 1) {
		m_slave = create_copy();
	}
	
	return 1;
}


int LV2Plugin::create_instance()
{
// 	printf("URI:\t%s\n", QS_C(m_pluginUri));

	LilvNode* plugin_uri = lilv_new_uri(PluginManager::instance()->get_lilv_world(), QS_C(m_pluginUri));
	m_plugin = lilv_plugins_get_by_uri(PluginManager::instance()->get_lilv_plugins(), plugin_uri);
	lilv_node_free(plugin_uri);

	
	if (! m_plugin) {
		fprintf(stderr, "Failed to find plugin %s.\n", QS_C(m_pluginUri));
		return -1;
	}
	
	/* Instantiate the plugin */
    uint samplerate = audiodevice().get_sample_rate();
    m_instance = lilv_plugin_instantiate(m_plugin, samplerate, nullptr);

	if (! m_instance) {
		printf("Failed to instantiate plugin.\n");
		return -1;
	} else {
// 		printf("Succesfully instantiated plugin.\n\n");
	}
	
	return 1;
}

void LV2Plugin::get_plugin_presets()
{
    /* Plugin presets */
    LilvWorld* world = PluginManager::instance()->get_lilv_world();
    
    LilvNode *label_uri  = lilv_new_uri(world, LILV_NS_RDFS "label");
    LilvNode *preset_uri = lilv_new_uri(world, LV2_PRESETS__Preset);
    LilvNodes *presets = lilv_plugin_get_related(m_plugin, preset_uri);
    if (presets)
    {
        LILV_FOREACH(nodes, i, presets)
        {
            const LilvNode *preset = lilv_nodes_get(presets, i);
            lilv_world_load_resource(world, preset);
            LilvNodes *labels = lilv_world_find_nodes(world, preset, label_uri, NULL);
            if (labels)
            {
                const LilvNode *label = lilv_nodes_get_first(labels);
                const QString sPreset(lilv_node_as_string(label));
                const QString sUri(lilv_node_as_string(preset));
                m_lv2_presets.insert(sPreset, sUri);
                lilv_nodes_free(labels);
            }
        }
        lilv_nodes_free(presets);
    }
    lilv_node_free(preset_uri);
    lilv_node_free(label_uri); 
}


void LV2Plugin::process(AudioBus* bus, nframes_t nframes)
{
	if ( is_bypassed() ) {
		return;
	}
	
	for (int i=0; i<m_audioInputPorts.size(); ++i) {
		AudioInputPort* port = m_audioInputPorts.at(i);
		int index = port->get_index();
		// If we are a slave, then we are meant to operate on the second channel of the Bus!
		if (m_isSlave) i = 1;
        lilv_instance_connect_port(m_instance, uint32_t(index), bus->get_buffer(i, nframes));
	}
	
	for (int i=0; i<m_audioOutputPorts.size(); ++i) {
		AudioOutputPort* port = m_audioOutputPorts.at(i);
		int index = port->get_index();
		// If we are a slave, then we are meant to operate on the second channel of the Bus!
		if (m_isSlave) i = 1;
        lilv_instance_connect_port(m_instance, uint32_t(index), bus->get_buffer(i, nframes));
	}
	
	/* Run plugin for this cycle */
	lilv_instance_run(m_instance, nframes);
	
	// If we have a slave, and the bus has 2 channels, process the slave too!
	if (m_slave && bus->get_channel_count() == 2) {
		m_slave->process(bus, nframes);
	}	
	
}


LV2ControlPort* LV2Plugin::create_port(uint32_t portIndex, float defaultValue)
{
    LV2ControlPort* ctrlport = nullptr;
    const LilvPort* slv2_port = lilv_plugin_get_port_by_index(m_plugin, portIndex);
	
	PortDirection direction;
        PortType type = UNKNOWN;

	if (lilv_port_is_a(m_plugin, slv2_port, m_input_class)) {
		direction = INPUT;
	} else if (lilv_port_is_a(m_plugin, slv2_port, m_output_class)) {
		direction = OUTPUT;
/*	} else if (slv2_port_has_property(m_plugin, slv2_port, m_optional)) {
		slv2_instance_connect_port(m_instance, port_index, NULL);*/
	} else {
		PERROR("Mandatory port has unknown type (neither input or output)");
		return ctrlport;
	}

	/* Set control values */
	if (lilv_port_is_a(m_plugin, slv2_port, m_control_class)) {
		type = CONTROL;
	} else if (lilv_port_is_a(m_plugin, slv2_port, m_audio_class)) {
		type = AUDIO;
	}/* else if (slv2_port_is_a(m_plugin, slv2_port, m_event_class)) {
		port->type = EVENT;
	}*/


	/* Create the port based on it's direction and type */
	switch (type) {
		case CONTROL:
			switch (direction) {
			case INPUT:
                defaultValue = std::isnan(defaultValue) ? 0.0 : defaultValue;
                ctrlport = new LV2ControlPort(this, int(portIndex), defaultValue);
				break;
			case OUTPUT:
                ctrlport = new LV2ControlPort(this, int(portIndex), 0);
				break;
			}
			break;
		case AUDIO:
			switch (direction) {
			case INPUT:
                m_audioInputPorts.append(new AudioInputPort(this, int(portIndex)));
				break;
			case OUTPUT:
                m_audioOutputPorts.append(new AudioOutputPort(this, int(portIndex)));
				break;
			}
			break;
		default:
			PERROR("ERROR: Unknown port data type!");
	}


	return ctrlport;
}

QString LV2Plugin::get_name( )
{
    LilvNode* name = lilv_plugin_get_name(m_plugin);
    
    QString s_name = "";
    if (name)
    {
        s_name = lilv_node_as_string(name);
        lilv_node_free(name);
    } 
    
    return s_name;
}




/*********************************************************/
/*		LV2 Control Port			 */
/*********************************************************/


LV2ControlPort::LV2ControlPort(LV2Plugin* plugin, int index, float value)
	: PluginControlPort(plugin, index, value)
	, m_lv2plugin(plugin)
{
    lilv_instance_connect_port(m_lv2plugin->get_instance(), uint32_t(m_index), &m_value);
	init();
}

LV2ControlPort::LV2ControlPort( LV2Plugin * plugin, const QDomNode node )
	: PluginControlPort(plugin, node)
	, m_lv2plugin(plugin)
{
    lilv_instance_connect_port(m_lv2plugin->get_instance(), uint32_t(m_index), &m_value);
	init();
}

void LV2ControlPort::init()
{
	foreach(const QString &string, get_hints()) {
		if (string == "http://lv2plug.in/ns/lv2core#logarithmic") {
                        m_hint = LOG_CONTROL;
		} else  if (string == "http://lv2plug.in/ns/lv2core#integer") {
                        m_hint = INT_CONTROL;
        }
    }
}

QDomNode LV2ControlPort::get_state( QDomDocument doc )
{
	return PluginControlPort::get_state(doc);
}


float LV2ControlPort::get_min_control_value()
{
    const LilvPort* port = lilv_plugin_get_port_by_index(m_lv2plugin->get_slv2_plugin(), uint32_t(m_index));
	LilvNode* minval;
    lilv_port_get_range(m_lv2plugin->get_slv2_plugin(), port, nullptr, &minval, nullptr);
	float val = lilv_node_as_float(minval);
        if (minval == nullptr) {
                return -1.0e6;
        }
	lilv_node_free(minval);
	return val;
}

float LV2ControlPort::get_max_control_value()
{
    const LilvPort* port = lilv_plugin_get_port_by_index(m_lv2plugin->get_slv2_plugin(), uint32_t(m_index));
	LilvNode* maxval;
    lilv_port_get_range(m_lv2plugin->get_slv2_plugin(), port, nullptr, nullptr, &maxval);
        if (maxval == nullptr) {
                return 1.0e6;
        }
	float val = lilv_node_as_float(maxval);
	lilv_node_free(maxval);
	return val;
}

float LV2ControlPort::get_default_value()
{
    const LilvPort* port = lilv_plugin_get_port_by_index(m_lv2plugin->get_slv2_plugin(), uint32_t(m_index));
	LilvNode* defval;
    lilv_port_get_range(m_lv2plugin->get_slv2_plugin(), port, &defval, nullptr, nullptr);
        if (defval == nullptr) {
                return this->get_min_control_value();
        }
	float val = lilv_node_as_float(defval);
	lilv_node_free(defval);
	return val;
}


QString LV2ControlPort::get_description()
{
    const LilvPort* port = lilv_plugin_get_port_by_index(m_lv2plugin->get_slv2_plugin(), uint32_t(m_index));
    
    LilvNode* portName = lilv_port_get_name(m_lv2plugin->get_slv2_plugin(), port);
    
    QString s_portName = "";
    if (portName)
    {
        s_portName = lilv_node_as_string(portName);
        lilv_node_free(portName);
    } 
    
    return s_portName;
}

QString LV2ControlPort::get_symbol()
{
    const LilvPort* port = lilv_plugin_get_port_by_index(m_lv2plugin->get_slv2_plugin(), uint32_t(m_index));
	return QString(lilv_node_as_string(lilv_port_get_symbol(m_lv2plugin->get_slv2_plugin(), port)));
}

QStringList LV2ControlPort::get_hints()
{
    const LilvPort* port = lilv_plugin_get_port_by_index(m_lv2plugin->get_slv2_plugin(), uint32_t(m_index));
	LilvNodes* values = lilv_port_get_properties(m_lv2plugin->get_slv2_plugin(), port);
	QStringList qslist;
	for (unsigned i=0; i < lilv_nodes_size(values); ++i) {
//		qslist << QString(lilv_node_as_string(lilv_nodes_get_at(values, i)));
	}
        
        lilv_nodes_free(values);
        
	return qslist;
}

LV2Plugin * LV2Plugin::create_copy()
{
	QDomDocument doc("LV2Plugin");
	QDomNode pluginState = get_state(doc);
        LV2Plugin* plug = new LV2Plugin(m_session, true);
	plug->set_state(pluginState);
	return plug;
}

TCommand * LV2Plugin::toggle_bypass()
{
	Plugin::toggle_bypass();
	if (m_slave) {
		m_slave->toggle_bypass();
	}
    return  nullptr;
}

PluginInfo LV2Plugin::get_plugin_info(const LilvPlugin* plugin)
{
    PluginInfo info;

    LilvNode *name = lilv_plugin_get_name(plugin);
    if (name)
    {
        info.name = lilv_node_as_string(name);
        lilv_node_free(name);
    } 

    info.uri = lilv_node_as_string(lilv_plugin_get_uri(plugin));

    LilvWorld* world = PluginManager::instance()->get_lilv_world();
    LilvNode* input = lilv_new_uri(world, LILV_URI_INPUT_PORT);
    LilvNode* output = lilv_new_uri(world, LILV_URI_OUTPUT_PORT);
    LilvNode* audio = lilv_new_uri(world, LILV_URI_AUDIO_PORT);
	
    info.audioPortInCount = int(lilv_plugin_get_num_ports_of_class(plugin, input, audio, nullptr));
    info.audioPortOutCount = int(lilv_plugin_get_num_ports_of_class(plugin, output, audio, nullptr));
    info.type = lilv_node_as_string(lilv_plugin_class_get_label(lilv_plugin_get_class(plugin)));

    lilv_node_free(input);
    lilv_node_free(output);
    lilv_node_free(audio);

    return info;
}

// Refresh and load preset labels listing. (virtual)
QStringList LV2Plugin::presetList (void) const
{
    QStringList list ; // preset uri

    QHash<QString, QString>::ConstIterator iter
        = m_lv2_presets.constBegin();
    const QHash<QString, QString>::ConstIterator& iter_end
        = m_lv2_presets.constEnd();
        
    for ( ; iter != iter_end; ++iter)
        list.append(iter.key());

    return list;
}

// Load plugin state from a named preset.
bool LV2Plugin::loadPreset ( const QString& sPreset)
{
    const QString& sUri = m_lv2_presets.value(sPreset);
    if (sUri.isEmpty())
        return false;

    LilvWorld* world = PluginManager::instance()->get_lilv_world();

    LilvNode *preset = lilv_new_uri(world, sUri.toUtf8().constData());

    LilvState *state = NULL;

    state = lilv_state_new_from_world(world, &g_lv2_urid_map, preset);

    if (state == NULL)
    {
        lilv_node_free(preset);
        return false;
    }

    lilv_state_restore(state, m_instance, traverso_lv2_set_port_value, this, 0, NULL);

    lilv_state_free(state);
    lilv_node_free(preset);

    return true;
}

//eof

