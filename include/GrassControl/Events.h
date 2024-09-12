#pragma once

#include "GrassControl/Config.h"
#include "GrassControl/DistantGrass.h"

class cellLoadEventHandler : public RE::BSTEventSink<RE::TESCellAttachDetachEvent>
{
public:
	static cellLoadEventHandler* GetSingleton();
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESCellAttachDetachEvent* a_event, RE::BSTEventSource<RE::TESCellAttachDetachEvent>* a_eventSource) override;
	static void Register();
};

class MenuOpenCloseEventHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	static MenuOpenCloseEventHandler* GetSingleton();
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;
	static void Register();
};
