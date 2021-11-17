// Widget.cpp

#include "../main.hpp"
#include "Widget.hpp"
#include "Frame.hpp"
#include "Image.hpp"
#include "../player.hpp"
#include "../input.hpp"
#include "../engine/audio/sound.hpp"

Widget::~Widget() {
	if (parent) {
		for (auto node = parent->widgets.begin(); node != parent->widgets.end(); ++node) {
			if (*node == this) {
				parent->widgets.erase(node);
				break;
			}
		}
	}
}

void Widget::select() {
	if (selected) {
		return;
	}
	Widget* head = findHead();
	if (head && head->getType() == WIDGET_FRAME) {
		Frame* f = static_cast<Frame*>(head);
		f->deselect(); // this deselects everything in the gui
	}
	selected = true;
}

void Widget::deselect() {
	selected = false;
}

void Widget::activate() {
	// no-op
}

void Widget::process() {
	if (!disabled) {
		if (tickCallback) {
			(*tickCallback)(*this);
		}
	}
}

Frame* Widget::findSearchRoot() {
	Widget* gui = findHead();
	if (gui && gui->getType() == WIDGET_FRAME) {
		if (widgetSearchParent.empty()) {
			return static_cast<Frame*>(gui);
		} else {
			auto search = gui->findWidget(widgetSearchParent.c_str(), true);
			if (search && search->getType() == WIDGET_FRAME) {
				return static_cast<Frame*>(search);
			} else {
				return static_cast<Frame*>(gui);
			}
		}
	} else {
		return nullptr;
	}
}

const Frame* Widget::findSearchRoot() const {
	const Widget* gui = findHead();
	if (gui && gui->getType() == WIDGET_FRAME) {
		if (widgetSearchParent.empty()) {
			return static_cast<const Frame*>(gui);
		} else {
			auto search = gui->findWidget(widgetSearchParent.c_str(), true);
			if (search && search->getType() == WIDGET_FRAME) {
				return static_cast<const Frame*>(search);
			} else {
				return static_cast<const Frame*>(gui);
			}
		}
	} else {
		return nullptr;
	}
}

Widget* Widget::handleInput() {
	if (selected && !inputstr) {
		Input& input = Input::inputs[owner];

		// find search root
		Frame* root = nullptr;

		// move to another widget
		for (auto& move : widgetMovements) {
			if (!move.second.empty()) {
				if (input.consumeBinaryToggle(move.first.c_str())) {
					root = root ? root : findSearchRoot();
					Widget* result = root->findWidget(move.second.c_str(), true);
					if (result && !result->disabled && !result->invisible) {
						auto in = input.input(move.first.c_str());
#ifndef EDITOR
						if (in.type != Input::binding_t::bindtype_t::MOUSE_BUTTON) {
							inputs.getVirtualMouse(owner)->draw_cursor = false;
						}
						playSound(495, 64);
#endif
						result->scrollParent();
						return result;
					}
				}
			}
		}

		// move to another widget and activate it
		for (auto& action : widgetActions) {
			if (!action.second.empty()) {
				if (input.consumeBinaryToggle(action.first.c_str())) {
					root = root ? root : findSearchRoot();
					Widget* result = root->findWidget(action.second.c_str(), true);
					if (result && !result->disabled) {
						auto in = input.input(action.first.c_str());
#ifndef EDITOR
						if (in.type != Input::binding_t::bindtype_t::MOUSE_BUTTON) {
							inputs.getVirtualMouse(owner)->draw_cursor = false;
						}
#endif
						result->activate();
						return nullptr;
					}
				}
			}
		}

		// activate current selection
		if (input.consumeBinaryToggle("MenuConfirm") && !disabled) {
			activate();
			return nullptr;
		}
	}
	return nullptr;
}

Widget* Widget::findHead() {
    if (parent && parent->owner == owner) {
        return parent->findHead();
    } else {
        return this;
    }
}

const Widget* Widget::findHead() const {
	if (parent && parent->owner == owner) {
		return parent->findHead();
	} else {
		return this;
	}
}

Widget* Widget::findWidget(const char* name, bool recursive) {
	for (auto widget : widgets) {
		if (widget->owner != owner) {
			continue;
		}
		if (widget->name == name) {
			return widget;
		} else if (recursive) {
			auto result = widget->findWidget(name, recursive);
			if (result) {
				return result;
			}
		}
	}
	return nullptr;
}

const Widget* Widget::findWidget(const char* name, bool recursive) const {
	for (auto widget : widgets) {
		if (widget->owner != owner) {
			continue;
		}
		if (widget->name == name) {
			return widget;
		} else if (recursive) {
			auto result = widget->findWidget(name, recursive);
			if (result) {
				return result;
			}
		}
	}
	return nullptr;
}

void Widget::findSelectedWidgets(std::vector<Widget*>& outResult) {
	if (selected) {
		outResult.push_back(this);
	}
	for (auto widget : widgets) {
		widget->findSelectedWidgets(outResult);
	}
}

void Widget::findSelectedWidgets(std::vector<const Widget*>& outResult) const {
	if (selected) {
		outResult.push_back(this);
	}
	for (auto widget : widgets) {
		widget->findSelectedWidgets(outResult);
	}
}

Widget* Widget::findSelectedWidget(int owner) {
	if (selected && owner == this->owner) {
		return this;
	} else {
		std::vector<Widget*> selectedWidgets;
		findSelectedWidgets(selectedWidgets);
		for (auto widget : selectedWidgets) {
			if (widget->owner == owner) {
				return widget;
			}
		}
	}
	return nullptr;
}

bool Widget::isChildOf(const Widget& widget) const {
	if (!parent) {
		return false;
	}
	else if (parent == &widget) {
		return true;
	}
	else {
		return parent->isChildOf(widget);
	}
}

void Widget::adoptWidget(Widget& widget) {
	if (widget.parent) {
		for (auto node = widget.parent->widgets.begin(); node != widget.parent->widgets.end(); ++node) {
			if (*node == &widget) {
				widget.parent->widgets.erase(node);
				break;
			}
		}
	}
	widget.owner = owner;
	widget.parent = this;
	widget.setOwner(this->getOwner());
	widgets.push_back(&widget);
}

void Widget::drawGlyphs(const SDL_Rect size, const std::vector<const Widget*>& selectedWidgets) const {
	if (drawCallback) {
		drawCallback(*this, size);
	}

	if (hideGlyphs) {
		return;
	}

	const SDL_Rect viewport{0, 0, Frame::virtualScreenX, Frame::virtualScreenY};
	const Widget* selectedWidget = nullptr;
	for (auto widget : selectedWidgets) {
		if (widget->owner == owner) {
			selectedWidget = widget;
			break;
		}
	}
	if (!selectedWidget) {
		return;
	} else {
		auto searchParent = selectedWidget->findSearchRoot();
		if (searchParent && !isChildOf(*searchParent)) {
			return;
		}
	}

	// draw selector widgets
	if (selectedWidget == this) {
		{
			auto image = Image::get("images/ui/Main Menus/Selector_TL.png");
			int w = image->getWidth();
			int h = image->getHeight();
			int x = size.x;
			int y = size.y;
			int beatx = (ticks % TICKS_PER_SECOND) < (TICKS_PER_SECOND / 2) ? w / 2 : w / 4;
			int beaty = (ticks % TICKS_PER_SECOND) < (TICKS_PER_SECOND / 2) ? h / 2 : h / 4;
			image->draw(nullptr, SDL_Rect{x - beatx, y - beaty, w, h}, viewport);
		}
		{
			auto image = Image::get("images/ui/Main Menus/Selector_TR.png");
			int w = image->getWidth();
			int h = image->getHeight();
			int x = size.x + size.w - w;
			int y = size.y;
			int beatx = (ticks % TICKS_PER_SECOND) < (TICKS_PER_SECOND / 2) ? w / 2 : w / 4;
			int beaty = (ticks % TICKS_PER_SECOND) < (TICKS_PER_SECOND / 2) ? h / 2 : h / 4;
			image->draw(nullptr, SDL_Rect{x + beatx, y - beaty, w, h}, viewport);
		}
		{
			auto image = Image::get("images/ui/Main Menus/Selector_BL.png");
			int w = image->getWidth();
			int h = image->getHeight();
			int x = size.x;
			int y = size.y + size.h - h;
			int beatx = (ticks % TICKS_PER_SECOND) < (TICKS_PER_SECOND / 2) ? w / 2 : w / 4;
			int beaty = (ticks % TICKS_PER_SECOND) < (TICKS_PER_SECOND / 2) ? h / 2 : h / 4;
			image->draw(nullptr, SDL_Rect{x - beatx, y + beaty, w, h}, viewport);
		}
		{
			auto image = Image::get("images/ui/Main Menus/Selector_BR.png");
			int w = image->getWidth();
			int h = image->getHeight();
			int x = size.x + size.w - w;
			int y = size.y + size.h - h;
			int beatx = (ticks % TICKS_PER_SECOND) < (TICKS_PER_SECOND / 2) ? w / 2 : w / 4;
			int beaty = (ticks % TICKS_PER_SECOND) < (TICKS_PER_SECOND / 2) ? h / 2 : h / 4;
			image->draw(nullptr, SDL_Rect{x + beatx, y + beaty, w, h}, viewport);
		}
	}

	// button prompts
#ifdef NINTENDO
	int x = size.x + size.w;
	int y = size.y + size.h;
	auto& actions = selectedWidget->getWidgetActions();
	auto action = actions.begin();
	if (selectedWidget == this) {
		auto image = Image::get("images/ui/Glyphs/G_Switch_A00.png");
		int w = image->getWidth();
		int h = image->getHeight();
		image->draw(nullptr, SDL_Rect{x - w / 2, y - h / 2, w, h}, viewport);
		x -= w;
	}
	if ((action = actions.find("MenuCancel")) != actions.end()) {
		if (action->second == name) {
			auto image = Image::get("images/ui/Glyphs/G_Switch_B00.png");
			int w = image->getWidth();
			int h = image->getHeight();
			image->draw(nullptr, SDL_Rect{x - w / 2, y - h / 2, w, h}, viewport);
			x -= w;
		}
	}
	if ((action = actions.find("MenuAlt1")) != actions.end()) {
		if (action->second == name) {
			auto image = Image::get("images/ui/Glyphs/G_Switch_Y00.png");
			int w = image->getWidth();
			int h = image->getHeight();
			image->draw(nullptr, SDL_Rect{x - w / 2, y - h / 2, w, h}, viewport);
			x -= w;
		}
	}
	if ((action = actions.find("MenuAlt2")) != actions.end()) {
		if (action->second == name) {
			auto image = Image::get("images/ui/Glyphs/G_Switch_X00.png");
			int w = image->getWidth();
			int h = image->getHeight();
			image->draw(nullptr, SDL_Rect{x - w / 2, y - h / 2, w, h}, viewport);
			x -= w;
		}
	}
	if ((action = actions.find("MenuStart")) != actions.end()) {
		if (action->second == name) {
			auto image = Image::get("images/ui/Glyphs/G_Switch_+00.png");
			int w = image->getWidth();
			int h = image->getHeight();
			image->draw(nullptr, SDL_Rect{x - w / 2, y - h / 2, w, h}, viewport);
			x -= w;
		}
	}
	if ((action = actions.find("MenuSelect")) != actions.end()) {
		if (action->second == name) {
			auto image = Image::get("images/ui/Glyphs/G_Switch_-00.png");
			int w = image->getWidth();
			int h = image->getHeight();
			image->draw(nullptr, SDL_Rect{x - w / 2, y - h / 2, w, h}, viewport);
			x -= w;
		}
	}
	if ((action = actions.find("MenuPageLeft")) != actions.end()) {
		if (action->second == name) {
			auto image = Image::get("images/ui/Glyphs/G_Switch_L00.png");
			int w = image->getWidth();
			int h = image->getHeight();
			image->draw(nullptr, SDL_Rect{x - w / 2, y - h / 2, w, h}, viewport);
			x -= w;
		}
	}
	if ((action = actions.find("MenuPageRight")) != actions.end()) {
		if (action->second == name) {
			auto image = Image::get("images/ui/Glyphs/G_Switch_R00.png");
			int w = image->getWidth();
			int h = image->getHeight();
			image->draw(nullptr, SDL_Rect{x - w / 2, y - h / 2, w, h}, viewport);
			x -= w;
		}
	}
#endif
}

void Widget::scrollParent() {
	// no-op
}