#include "pch.h"
#include "hMainMenu.h"




hMainMenu::~hMainMenu()
{
	for (int i = 0; i < menuItemsCount; i++) {
		delete menuItems[i];
	}

}




hMenuItem hMainMenu::nextMenuItem()
{
	selectedItem++;
	if (selectedItem>=menuItemsCount) {
		selectedItem = 0;
	}
	return *menuItems[selectedItem];
}

/*
hMenuItem::hMenuItem(char* caption[])
{
	strcpy(this->caption, caption);
}
*/
