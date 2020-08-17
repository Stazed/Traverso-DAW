/*
Copyright (C) 2006-2019 Remon Sijrier

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
 
#include "PluginChain.h"

#include "Plugin.h"
#include "PluginManager.h"
#include "TInputEventDispatcher.h"
#include "TSession.h"
#include "AddRemove.h"
#include "GainEnvelope.h"
#include "Information.h"
#include "Utils.h"  // for QS_C() - FIXME remove when done

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"

PluginChain::PluginChain(ContextItem* parent, TSession* session)
	: ContextItem(parent)
{
    m_fader = new GainEnvelope(session);
    private_add_plugin(m_fader);
    private_plugin_added(m_fader);

    set_session(session);

    connect(this, SIGNAL(privatePluginAdded(Plugin*)), this, SLOT(private_plugin_added(Plugin*)));
    connect(this, SIGNAL(privatePluginRemoved(Plugin*)), this, SLOT(private_plugin_removed(Plugin*)));
    connect(this, SIGNAL(privatePluginOrderChanged(Plugin*)), this, SLOT(private_plugin_order_changed(Plugin*)));
    connect(this, SIGNAL(privatePluginReverseChange(Plugin*)), this, SLOT(private_plugin_reverse_change(Plugin*)));
}

PluginChain::~ PluginChain()
{
	PENTERDES;
    for(auto plugin : m_plugins) {
        delete plugin;
	}
}


QDomNode PluginChain::get_state(QDomDocument doc)
{
	QDomNode pluginsNode = doc.createElement("Plugins");
	
    for(Plugin* plugin : m_plugins) {
        if (plugin == m_fader) {
            continue;
        }
		pluginsNode.appendChild(plugin->get_state(doc));
	}
	
    pluginsNode.appendChild(m_fader->get_state(doc));
	
	return pluginsNode;
}

int PluginChain::set_state( const QDomNode & node )
{
	QDomNode pluginsNode = node.firstChildElement("Plugins");
	QDomNode pluginNode = pluginsNode.firstChild();
	
	while(!pluginNode.isNull()) {
		if (pluginNode.toElement().attribute( "type", "") == "GainEnvelope") {
			m_fader->set_state(pluginNode);
		} else {
			Plugin* plugin = PluginManager::instance()->get_plugin(pluginNode);
			if (!plugin) {
				pluginNode = pluginNode.nextSibling();
				continue;
			}
			plugin->set_history_stack(get_history_stack());
			private_add_plugin(plugin);
            private_plugin_added(plugin);
		}
		
		pluginNode = pluginNode.nextSibling();
	}
	
	return 1;
}


TCommand* PluginChain::add_plugin(Plugin * plugin, bool historable)
{
    plugin->set_history_stack(get_history_stack());

    return new AddRemove( this, plugin, historable, m_session,
                          "private_add_plugin(Plugin*)", "privatePluginAdded(Plugin*)",
                          "private_remove_plugin(Plugin*)", "privatePluginRemoved(Plugin*)",
                          tr("Add Plugin (%1)").arg(plugin->get_name()));
}


TCommand* PluginChain::remove_plugin(Plugin* plugin, bool historable)
{
    if (plugin == m_fader) {
        // do not remove fader we always have one
        info().information(tr("Gain Envelope (Fader) is not removable"));
        return ied().failure();
    }

    return new AddRemove( this, plugin, historable, m_session,
                          "private_remove_plugin(Plugin*)", "privatePluginRemoved(Plugin*)",
                          "private_add_plugin(Plugin*)", "privatePluginAdded(Plugin*)",
                          tr("Remove Plugin (%1)").arg(plugin->get_name()));
}

TCommand* PluginChain::change_plugin_order(Plugin* plugin, bool historable)
{
    return new AddRemove( this, plugin, historable, m_session,
                        "private_change_plugin_order(Plugin*)", "privatePluginOrderChanged(Plugin*)",
                        "private_reverse_plugin_change(Plugin*)", "privatePluginReverseChange(Plugin*)",
                        tr("Re-order Plugin (%1)").arg(plugin->get_name()));
}

void PluginChain::private_add_plugin( Plugin * plugin )
{
    if(plugin->is_prefader())
    {
        int faderIndex = m_rtPlugins.indexOf(m_fader);
        
        if(faderIndex <= 0)
        {
            // insert before m_fader
            m_rtPlugins.prepend(plugin);
        }
        else
        {
            // insert after last prefader plugin
            m_rtPlugins.insert(m_rtPlugins.at(faderIndex - 1), plugin);
        }
    }
    else
    {
        m_rtPlugins.append(plugin);
    }
}


void PluginChain::private_remove_plugin( Plugin * plugin )
{
    if (!m_rtPlugins.remove(plugin)) {
		PERROR("Plugin not found in list, this is invalid plugin remove!!!!!");
    }
}

void PluginChain::private_change_plugin_order( Plugin * plugin )
{
    int first = m_rtPlugins.indexOf(plugin);
    int second = 0;
    int size = m_rtPlugins.size();
    
    if(plugin->is_reorder_up())
    {
        second = first - 1;
        
        if(second < 0)
            return;     // we are already at the beginning
    }
    else
    {
        second = first + 1;
        
        if(second >= size)
            return;     // we are already at the end
    }
    
    // do the swap
    m_rtPlugins.swap(m_rtPlugins.at(first), m_rtPlugins.at(second));
}

void PluginChain::private_plugin_order_changed( Plugin * plugin )
{
    int first = m_plugins.indexOf(plugin);
    int second = 0;
    int size = m_plugins.size();
    
    if(plugin->is_reorder_up())
    {
        second = first - 1;
        
        if(second < 0)
            return;     // we are already at the beginning
    }
    else
    {
        second = first + 1;
        
        if(second >= size)
            return;     // we are already at the end
    }
    
    // do the swap
    m_plugins.swap(first, second);

    // We allow the plugin to be moved from pre to post fader and visa versa
    if(m_plugins.indexOf(m_fader) > m_plugins.indexOf(plugin))
    {
        // needed for file saving and loading
        plugin->set_prefader(true);
    }
    else
    {
        // needed for file saving and loading
        plugin->set_prefader(false);
    }
}

void PluginChain::private_reverse_plugin_change( Plugin * plugin )
{
    // Not used
}

void PluginChain::private_plugin_reverse_change( Plugin * plugin )
{
    // Not used
}

void PluginChain::private_plugin_added(Plugin *plugin)
{
    if(plugin->is_prefader())
    {
        int faderIndex = m_plugins.indexOf(m_fader);
        
        if(faderIndex <= 0)
        {
            // insert at beginning
            m_plugins.prepend(plugin);
        }
        else
        {
            // insert before prefader plugin
            m_plugins.insert(faderIndex, plugin);
        }
    }
    else
    {
        m_plugins.append(plugin);
    }
    
    
    emit pluginAdded(plugin);
}

void PluginChain::private_plugin_removed(Plugin *plugin)
{
    m_plugins.removeAll(plugin);
    emit pluginRemoved(plugin);
}

void PluginChain::set_session(TSession * session)
{
    if (!session) {
        return;
    }

    m_session = session;
    set_history_stack(m_session->get_history_stack());
    m_fader->set_session(session);
}

QList<Plugin *> PluginChain::get_pre_fader_plugins()
{
    QList<Plugin*> preFaderPlugins;
    for(Plugin* plugin : m_plugins) {
        if (plugin == m_fader) {
            return preFaderPlugins;
        } else {
            preFaderPlugins.append(plugin);
        }
    }

    return preFaderPlugins;
}

QList<Plugin *> PluginChain::get_post_fader_plugins()
{
    QList<Plugin*> postFaderPlugins;
    bool faderWasReached = false;

    for(Plugin* plugin : m_plugins) {
        if (faderWasReached) {
            postFaderPlugins.append(plugin);
        } else if (plugin == m_fader) {
            faderWasReached = true;
        }
    }

    return postFaderPlugins;
}

