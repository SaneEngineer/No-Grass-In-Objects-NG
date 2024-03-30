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
	    if (a_event->menuName == RE::MainMenu::MENU_NAME) {
			if (a_event->opening) {
				GrassControl::GrassControlPlugin::OnMainMenuOpen();
			} 
		} 
	}

	return RE::BSEventNotifyControl::kContinue;
}
