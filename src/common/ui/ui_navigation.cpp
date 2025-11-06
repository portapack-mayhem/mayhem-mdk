#include "ui_navigation.hpp"
#include "file_reader.hpp"

namespace ui {

View* NavigationView::push_view(std::unique_ptr<View> new_view) {
    free_view();
    const auto p = new_view.get();
    view_stack.emplace_back(ViewState{std::move(new_view), {}});

    update_view();
    return p;
}

void NavigationView::pop(bool trigger_update) {
    // Don't pop off the NavView.
    if (view_stack.size() <= 1)
        return;

    auto on_pop = view_stack.back().on_pop;

    free_view();
    view_stack.pop_back();

    // NB: These are executed _after_ the view has been
    // destroyed. The old view MUST NOT be referenced in
    // these callbacks or it will cause crashes.
    if (trigger_update) update_view();
    if (on_pop) on_pop();
}

void NavigationView::free_view() {
    // The focus_manager holds a raw pointer to the currently focused Widget.
    // It then tries to call blur() on that instance when the focus is changed.
    // This causes crashes if focused_widget has been deleted (as is the case
    // when a view is popped). Calling blur() here resets the focus_manager's
    // focus_widget pointer so focus can be called safely.
    this->blur();
    remove_child(view());
}

Widget* NavigationView::view() const {
    return children_.empty() ? nullptr : children_[0];
}

void NavigationView::update_view() {
    const auto& top = view_stack.back();
    auto top_view = top.view.get();

    add_child(top_view);
    auto newSize = (is_top()) ? Size{size().width(), size().height() - 16} : size();  // if top(), then there is the info bar at the bottom, so leave space for it
    top_view->set_parent_rect({{0, 0}, newSize});
    focus();
    set_dirty();

    // if (on_view_changed) on_view_changed(*top_view);
}

bool NavigationView::is_top() const {
    return view_stack.size() == 1;
}

bool NavigationView::is_valid() const {
    return view_stack.size() != 0;  // work around to check if nav is valid, not elegant i know. so TODO
}

bool NavigationView::set_on_pop(std::function<void()> on_pop) {
    if (view_stack.size() <= 1)
        return false;

    auto& top = view_stack.back();
    if (top.on_pop)
        return false;

    top.on_pop = on_pop;
    return true;
}

void NavigationView::display_modal(
    const std::string& title,
    const std::string& message) {
    display_modal(title, message, INFO, nullptr);
}

void NavigationView::display_modal(
    const std::string& title,
    const std::string& message,
    modal_t type,
    std::function<void(bool)> on_choice,
    bool compact) {
    push<ModalMessageView>(title, message, type, on_choice, compact);
}

/* ModalMessageView ******************************************************/

ModalMessageView::ModalMessageView(
    NavigationView& nav,
    const std::string& title,
    const std::string& message,
    modal_t type,
    std::function<void(bool)> on_choice,
    bool compact)
    : title_{title},
      message_{message},
      type_{type},
      on_choice_{on_choice},
      compact{true} {
    (void)compact;  // no bitmaps for now
    if (type == INFO) {
        add_child(&button_ok);
        button_ok.on_select = [this, &nav](Button&) {
            if (on_choice_) on_choice_(true);
            nav.pop();
        };

    } else if (type == YESNO) {
        add_children({&button_yes,
                      &button_no});

        button_yes.on_select = [this, &nav](Button&) {
            if (on_choice_) on_choice_(true);
            nav.pop();
        };
        button_no.on_select = [this, &nav](Button&) {
            if (on_choice_) on_choice_(false);
            nav.pop();
        };

    } else {  // ABORT
        add_child(&button_ok);

        button_ok.on_select = [this, &nav](Button&) {
            if (on_choice_) on_choice_(true);
            nav.pop(false);  // Pop the modal.
            nav.pop();       // Pop the underlying view.
        };
    }
}

void ModalMessageView::paint(Painter& painter) {
    /*if (!compact) painter.draw_bitmap({UI_POS_X_CENTER(6),
                                       UI_POS_Y(4)},
                                      bitmap_icon_utilities.size,
                                      bitmap_icon_utilities.data,
                                      Theme::getInstance()->bg_darkest->foreground,
                                      Theme::getInstance()->bg_darkest->background, 3);*/

    // Break lines.
    auto lines = split_string(message_, '\n');
    for (size_t i = 0; i < lines.size(); ++i) {
        painter.draw_string(
            {1 * 8, (Coord)(((compact) ? 8 * 3 : 120) + (i * 16))},
            style(),
            lines[i]);
    }
}

void ModalMessageView::focus() {
    if ((type_ == YESNO)) {
        button_yes.focus();
    } else {
        button_ok.focus();
    }
}

}  // namespace ui