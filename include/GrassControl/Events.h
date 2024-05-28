#pragma once

#include "GrassControl/Config.h"
#include "GrassControl/DistantGrass.h"

class cellLoadEventHandler : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>
{
public:
	static cellLoadEventHandler* GetSingleton();
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>* a_eventSource);
	static void Register();
};

class MenuOpenCloseEventHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	static MenuOpenCloseEventHandler* GetSingleton();
	virtual RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;
	static void Register();
};
