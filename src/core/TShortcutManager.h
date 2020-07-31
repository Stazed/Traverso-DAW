/*
Copyright (C) 2011 Remon Sijrier

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

#ifndef TSHORTCUTMANAGER_H
#define TSHORTCUTMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QStringList>

class CommandPlugin;
class TCommand;

class TFunction {

public:
	static bool smaller(const TFunction* left, const TFunction* right )
	{
		return left->sortorder < right->sortorder;
	}
	static bool greater(const TFunction* left, const TFunction* right )
	{
		return left->sortorder > right->sortorder;
	}

	TFunction() {
        m_inheritedFunction = nullptr;
        // used for cursor hinting. usually means displaying a horizontal arrow cursor
        // to indicate horizontal movement
		useX = false;
        // used for cursor hinting. usually means displaying a vertical arrow cursor
        // to indicate vertical movement
        useY = false;
		sortorder = 0;
		m_usesAutoRepeat = false;
		m_autorepeatInterval = -1;
		m_autorepeatStartDelay = -1;
		m_usesInheritedBase = false;
	}

    QString getKeySequence(bool formatHtml=false);
	QString getModifierSequence(bool fromInheritedBase=true);
	QString getSlotSignature() const;
	QString getDescription() const;
	QString getInheritedBase() const {return m_inheritedBase;}
	QString getLongDescription() const;
	QList<int> getModifierKeys(bool fromInheritedBase=true);
	QStringList getKeys(bool fromInheritedBase=true) const;
	QStringList getObjects() const;
	QString getObject() const;

	int getAutoRepeatInterval() const;
	int getAutoRepeatStartDelay() const;
	TFunction* getInheritedFunction() const {return m_inheritedFunction;}


	bool usesAutoRepeat() const {return m_usesAutoRepeat;}
	bool usesInheritedBase() const {return m_usesInheritedBase;}

	void setDescription(const QString& des);
	void setInheritedBase(const QString& base);
	void setUsesInheritedbase(bool b) {m_usesInheritedBase = b;}
    void setAutoRepeatInterval(int interval) {m_autorepeatInterval = interval; m_usesAutoRepeat = true;}
	void setAutoRepeatStartDelay(int delay) {m_autorepeatStartDelay = delay;}
	void setSlotSignature(const QString& signature) {slotsignature = signature;}

	QVariantList arguments;
	QString object;
	QString pluginname;
	QString commandName;
	QString submenu;
	bool useX;
	bool useY;
	int sortorder;

private:
	QStringList	m_keys;
	QString		slotsignature;
	QString		m_description;
	QString		m_inheritedBase;
	TFunction*	m_inheritedFunction;
	QList<int >	m_modifierkeys;
	int		m_autorepeatInterval;
	int		m_autorepeatStartDelay;
	bool		m_usesAutoRepeat;
	bool		m_usesInheritedBase;

	void setInheritedFunction(TFunction* inherited);


    friend class TShortcutManager;
};

class TShortcut
{
public:
	TShortcut(int keyValue)
	{
		m_keyValue = keyValue;
	}
	~ TShortcut() {}

	int getKeyValue() const {return m_keyValue;}

	QList<TFunction*> getFunctionsForObject(const QString& objectName);
	QList<TFunction*> getFunctions();

	int		autorepeatInterval;
	int		autorepeatStartDelay;

private:
	QMultiHash<QString, TFunction*> objects;
	int		m_keyValue;


	friend class TShortcutManager;
};


class TShortcutManager : public QObject
{
	Q_OBJECT
public:

    ~TShortcutManager();
    void createAndAddFunction(const QString &object, const QString &description, const QString &slotSignature, const QString &commandName, const QString& inheritedBase = "");
	void registerFunction(TFunction* function);
	TFunction* getFunction(const QString& function) const;

    QList<TFunction* > getFunctionsFor(QString className);
	TShortcut* getShortcutForKey(const QString& key);
	TShortcut* getShortcutForKey(int key);
	CommandPlugin* getCommandPlugin(const QString& pluginName);
	void modifyFunctionKeys(TFunction* function, const QStringList& keys, QStringList modifiers);
	void modifyFunctionInheritedBase(TFunction* function, bool usesInheritedBase);
	void add_translation(const QString& signature, const QString& translation);
	void add_meta_object(const QMetaObject* mo);
	void registerItemClass(const QString& item, const QString& className);
    void register_command_plugin(CommandPlugin* plugin, const QString& pluginName);
	QString get_translation_for(const QString& entry);
    QString createHtmlForClass(const QString& className, QObject* obj=nullptr);
	QList<QString> getClassNames() const;
	QString getClassForObject(const QString& object) const;
	bool classInherits(const QString& className, const QString &inherited);

	void loadFunctions();
	void saveFunction(TFunction* function);
	void exportFunctions();
	void loadShortcuts();
	void restoreDefaultFor(TFunction* function);
	void restoreDefaults();
    static void makeShortcutKeyHumanReadable(QString& key, bool formatHtml=false);

	bool isCommandClass(const QString& className);

private:
	QHash<QString, CommandPlugin*>	m_commandPlugins;

	QHash<QString, TFunction*>	m_functions;
	QHash<int, TShortcut*>		m_shortcuts;
	QHash<QString, QString>		m_translations;
	QHash<QString, QList<const QMetaObject*> > m_metaObjects;

    // be sure to only insert into m_classes using registerItemClass()
    // to avoid overwriting existing entries
    QMap<QString, QStringList>	m_classes;

	TShortcutManager();
	TShortcutManager(const TShortcutManager&) : QObject() {}

	friend TShortcutManager& tShortCutManager();

public slots:
	TCommand* export_keymap();
	TCommand* get_keymap(QString &);

signals:
	void functionKeysChanged();
};

TShortcutManager& tShortCutManager();


#endif // TSHORTCUTMANAGER_H
