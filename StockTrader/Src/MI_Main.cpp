#include "pch.h"
#include "MenuItems/Mi_Main.h"

namespace jv::bt
{
	bool MI_Main::Update(STBT& stbt, uint32_t& index)
	{
		ImGui::Begin("Menu", nullptr, WIN_FLAGS);
		ImGui::SetWindowPos({ 0, 0 });
		ImGui::SetWindowSize({ MENU_RESOLUTION_LARGE.x, MENU_RESOLUTION_LARGE.y });

		ImGui::Text(GetMenuTitle());
		ImGui::Text(GetDescription());
		ImGui::Dummy({ 0, 20 });

		bool quit = DrawMainMenu(index);
		if (quit)
			return true;

		if (index != 0 && ImGui::Button("Back"))
			index = 0;

		ImGui::End();

		if (auto subMenuTitle = GetSubMenuTitle())
		{
			ImGui::Begin(subMenuTitle, nullptr, WIN_FLAGS);
			ImGui::SetWindowPos({ MENU_WIDTH, 0 });
			ImGui::SetWindowSize({ MENU_RESOLUTION_LARGE.x, MENU_RESOLUTION_LARGE.y });
			quit = DrawSubMenu(index);
			if (quit)
				return true;
			ImGui::End();
		}

		return false;
	}
	bool MI_Main::DrawSubMenu(uint32_t& index)
	{
		return false;
	}
}