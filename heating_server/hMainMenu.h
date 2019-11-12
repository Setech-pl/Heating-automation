#pragma once
#pragma warning(disable : 4200)


class hMenuItem {
	hMenuItem(char *caption[18]);

protected:
	char* caption[];

};

class hMainMenu
{
public:
	hMainMenu();
	~hMainMenu();
	hMenuItem nextMenuItem();
	hMenuItem previousMenuItem();
	hMenuItem selectedMenuItem();

protected:
		int selectedItem;
		hMenuItem* menuItems[16];
		int menuItemsCount;
};
