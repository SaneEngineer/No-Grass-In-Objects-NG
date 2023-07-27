#include "GrassControl/Events.h"

MenuOpenCloseEventHandler* MenuOpenCloseEventHandler::GetSingleton()
{
	static MenuOpenCloseEventHandler singleton;
	return &singleton;
}

void MenuOpenCloseEventHandler::Register()
{
	RE::UI::GetSingleton()->AddEventSink(GetSingleton());
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
