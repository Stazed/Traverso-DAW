/*
    Copyright (C) 2006-2007 Remon Sijrier

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

#ifndef PLUGIN_VIEW_H
#define PLUGIN_VIEW_H


#include "ViewItem.h"
#include <QString>

class PluginPropertiesDialog;

class AudioTrackView;
class PluginChainView;
class Plugin;
class PluginChain;

class PluginView : public ViewItem
{
    Q_OBJECT

public:
    PluginView(PluginChainView* pcv, PluginChain* chain, Plugin* plugin, int index);
    ~PluginView();

    Plugin* get_plugin();
    void set_index(int index);
    void set_moving(bool move);
    void set_center(qreal center){m_center = center;}
    qreal get_center(){return m_center;}
    PluginChain* get_plugin_chain(){return m_pluginchain;}
    

    void paint(QPainter* painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    void calculate_bounding_rect();

private:
    PluginChain*	m_pluginchain;
    Plugin*         m_plugin;

    int             m_index;
    bool            m_moving;
    int             m_textwidth;
    qreal           m_center;
    QString         m_name;

    PluginPropertiesDialog* m_propertiesDialog;

public slots:
    TCommand* edit_properties();
    TCommand* remove_plugin();

private slots:
    void repaint();
    
signals:
    void pluginMove(PluginView*);

};

#endif

//eof

