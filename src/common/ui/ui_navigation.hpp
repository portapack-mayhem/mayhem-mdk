#pragma once

#include "ui_widget.hpp"
#include "ui_helper.hpp"
namespace ui {

enum modal_t {
    INFO = 0,
    YESNO,
    ABORT
};

class NavigationView : public ui::View {
   public:
    NavigationView(ui::Context& context, const ui::Rect parent_rect)
        : View{parent_rect}, context_(context) {
        set_style(ui::Theme::getInstance()->bg_dark);
    }

    void display_modal(const std::string& title, const std::string& message);
    void display_modal(
        const std::string& title,
        const std::string& message,
        modal_t type,
        std::function<void(bool)> on_choice = nullptr,
        bool compact = false);

    template <class T, class... Args>
    T* push(Args&&... args) {
        return reinterpret_cast<T*>(push_view(std::unique_ptr<View>(new T(*this, std::forward<Args>(args)...))));
    }

    template <class T, class... Args>
    T* replace(Args&&... args) {
        pop();
        return reinterpret_cast<T*>(push_view(std::unique_ptr<View>(new T(*this, std::forward<Args>(args)...))));
    }

    void push(View* v);
    View* push_view(std::unique_ptr<View> new_view);
    void replace(View* v);
    void pop(bool trigger_update = true);
    bool set_on_pop(std::function<void()> on_pop);
    ui::Context& context() const override {
        return context_;
    }
    bool is_top() const;
    bool is_valid() const;

    std::function<void()> on_navigate_back{};

   protected:
    ui::Context& context_;
    struct ViewState {
        std::unique_ptr<View> view;
        std::function<void()> on_pop;
    };

    std::vector<ViewState> view_stack{};

    Widget* view() const;

    void free_view();
    void update_view();
};

class ModalMessageView : public View {
   public:
    ModalMessageView(
        NavigationView& nav,
        const std::string& title,
        const std::string& message,
        modal_t type,
        std::function<void(bool)> on_choice,
        bool compact = false);

    void paint(Painter& painter) override;
    void focus() override;

    std::string title() const override { return title_; };

   private:
    const std::string title_;
    const std::string message_;
    const modal_t type_;
    const std::function<void(bool)> on_choice_;
    const bool compact;

    Button button_ok{
        {UI_POS_X_CENTER(10), UI_POS_Y_BOTTOM(5), UI_POS_WIDTH(10), UI_POS_HEIGHT(3)},
        "OK",
    };

    Button button_yes{
        {UI_POS_X_CENTER(8) - UI_POS_WIDTH(6), UI_POS_Y_BOTTOM(5), UI_POS_WIDTH(8), UI_POS_HEIGHT(3)},
        "YES",
    };

    Button button_no{
        {UI_POS_X_CENTER(8) + UI_POS_WIDTH(6), UI_POS_Y_BOTTOM(5), UI_POS_WIDTH(8), UI_POS_HEIGHT(3)},
        "NO",
    };
};

}  // namespace ui