#include "GrassControl/Events.h"

cellLoadEventHandler* cellLoadEventHandler::GetSingleton()
{
    static cellLoadEventHandler singleton;
    return &singleton;
}

MenuOpenCloseEventHandler* MenuOpenCloseEventHandler::GetSingleton()
{
	static MenuOpenCloseEventHandler singleton;
	return &singleton;
}

void MenuOpenCloseEventHandler::Register()
{
	RE::UI::GetSingleton()->AddEventSink(GetSingleton());
}

void cellLoadEventHandler::Register()
{
	RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(GetSingleton());
}

RE::BSEventNotifyControl cellLoadEventHandler::ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
{
	if (a_event) {
		if(a_event->cell->IsInteriorCell()) {
		    if (GrassControl::Config::OnlyLoadFromCache) {
			     GrassControl::DistantGrass::LO2Map->UnloadAll();
		    }
		}
	}
	
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl MenuOpenCloseEventHandler::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (a_event) {
#ifndef NDEBUG
		if (a_event->menuName == RE::Console::MENU_NAME) {
#else
		if (a_event->menuName == RE::MainMenu::MENU_NAME) {
#endif
			if (a_event->opening) {
				GrassControl::GrassControlPlugin::OnMainMenuOpen();
			}
		}
	}

	return RE::BSEventNotifyControl::kContinue;
}
