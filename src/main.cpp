#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "shell.h"
#include "file_system.h"

namespace
{
    const int WINDOW_WIDTH = 1000;
    const int WINDOW_HEIGHT = 700;
    const int FONT_SIZE = 18;
    const int LINE_HEIGHT = 24; // 恢复常量
    const int PADDING = 14;     // 恢复常量

    struct RenderedLine
    {
        SDL_Texture *texture = nullptr;
        int width = 0;
        int height = 0;
        ~RenderedLine()
        {
            if (texture)
                SDL_DestroyTexture(texture);
        }
    };

    // 纯白文字（无描边）
    std::unique_ptr<RenderedLine> render_text(SDL_Renderer *renderer, TTF_Font *font, const std::string &text)
    {
        if (text.empty())
            return nullptr;
        TTF_SetFontOutline(font, 0);
        SDL_Color white{255, 255, 255, 255};
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text.c_str(), white);
        if (!surf)
            return nullptr;
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        auto rl = std::make_unique<RenderedLine>();
        rl->texture = tex;
        rl->width = surf->w;
        rl->height = surf->h;
        SDL_FreeSurface(surf);
        return rl;
    }

    // 信息区域：纯黄色，无描边（你之前的要求）
    std::unique_ptr<RenderedLine> render_text_info(SDL_Renderer *renderer, TTF_Font *font, const std::string &text)
    {
        if (text.empty())
            return nullptr;
        TTF_SetFontOutline(font, 0);
        SDL_Color yellow{255, 255, 0, 255};
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text.c_str(), yellow);
        if (!surf)
            return nullptr;
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        auto rl = std::make_unique<RenderedLine>();
        rl->texture = tex;
        rl->width = surf->w;
        rl->height = surf->h;
        SDL_FreeSurface(surf);
        return rl;
    }

    // 批量压入“信息区域”输出（黄色）
    void push_output_lines_info(SDL_Renderer *renderer, TTF_Font *font, const std::string &text,
                                std::vector<std::unique_ptr<RenderedLine>> &logLines)
    {
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto rl = render_text_info(renderer, font, line);
            if (rl)
                logLines.emplace_back(std::move(rl));
        }
    }

    // 将多行文本压入日志（白字）
    void push_output_lines(SDL_Renderer *renderer, TTF_Font *font,
                           const std::string &text,
                           std::vector<std::unique_ptr<RenderedLine>> &logLines)
    {
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto rl = render_text(renderer, font, line);
            if (rl)
                logLines.emplace_back(std::move(rl));
        }
    }

} // namespace

int main()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    if (TTF_Init() == -1)
    {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n";
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << "\n";
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    SDL_Window *window = SDL_CreateWindow("LCX` terminal",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 字体
    TTF_Font *font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", FONT_SIZE);
    if (!font)
    {
        font = TTF_OpenFont("/usr/share/fonts/liberation/LiberationMono-Regular.ttf", FONT_SIZE);
        if (!font)
        {
            std::cerr << "Load font failed\n";
            return 1;
        }
    }

    // 背景图片
    SDL_Texture *background = IMG_LoadTexture(renderer, "/home/zns/code/终端背景.png");
    if (!background)
        std::cerr << "Background load failed: " << IMG_GetError() << "\n";

    // 文件系统 + Shell
    std::ostringstream initCap;
    std::streambuf *oldBuf = std::cout.rdbuf(initCap.rdbuf());
    FileSystem fs;
    fs.mount();
    std::cout.rdbuf(oldBuf);
    Shell shell(&fs);

    // 终端状态
    std::string input_text;
    std::vector<std::unique_ptr<RenderedLine>> logLines;
    std::vector<std::string> history;
    int history_index = -1;    // -1 当前输入
    float scrollOffset = 0.0f; // 0 = 底部
    bool quit = false;
    SDL_Event e;

    // 初始化输出：信息区域（黄色）
    push_output_lines_info(renderer, font, initCap.str(), logLines);

    SDL_StartTextInput();

    while (!quit)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
            else if (e.type == SDL_TEXTINPUT)
            {
                input_text += e.text.text;
            }
            else if (e.type == SDL_KEYDOWN)
            {
                const SDL_Keycode key = e.key.keysym.sym;
                const SDL_Keymod mod = static_cast<SDL_Keymod>(e.key.keysym.mod);

                if (key == SDLK_BACKSPACE)
                {
                    if (!input_text.empty())
                    {
                        const char *s = input_text.c_str();
                        int i = (int)input_text.size() - 1;
                        while (i > 0 && (s[i] & 0xC0) == 0x80)
                            --i; // UTF-8 退格
                        input_text.erase((size_t)i);
                    }
                }
                else if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
                {
                    // 回显命令（白字）
                    std::string prompt = fs.getCurrentPath() + " $ " + input_text;
                    push_output_lines(renderer, font, prompt, logLines);

                    // 执行命令并输出为信息区域（黄色）
                    std::ostringstream cmdCap;
                    std::streambuf *bufOld = std::cout.rdbuf(cmdCap.rdbuf());
                    shell.executeCommandPublic(input_text);
                    std::cout.rdbuf(bufOld);
                    push_output_lines_info(renderer, font, cmdCap.str(), logLines);

                    if (!input_text.empty() && (history.empty() || history.back() != input_text))
                        history.push_back(input_text);
                    history_index = -1;
                    input_text.clear();
                    scrollOffset = 0.0f; // 滚到底部
                }
                else if (key == SDLK_UP)
                {
                    if (!history.empty())
                    {
                        if (history_index == -1)
                            history_index = (int)history.size() - 1;
                        else if (history_index > 0)
                            --history_index;
                        input_text = history[history_index];
                    }
                }
                else if (key == SDLK_DOWN)
                {
                    if (!history.empty() && history_index != -1)
                    {
                        ++history_index;
                        if (history_index >= (int)history.size())
                        {
                            history_index = -1;
                            input_text.clear();
                        }
                        else
                            input_text = history[history_index];
                    }
                }
                else if (key == SDLK_c && (mod & KMOD_CTRL))
                {
                    // Ctrl+C：显示 ^C 并清空输入
                    push_output_lines(renderer, font, "^C", logLines);
                    input_text.clear();
                    scrollOffset = 0.0f;
                }
            }
            else if (e.type == SDL_MOUSEWHEEL)
            {
                // 滚动查看历史
                scrollOffset += e.wheel.y * LINE_HEIGHT * 3;
                if (scrollOffset < 0.0f)
                    scrollOffset = 0.0f;
            }
            // 去掉了鼠标中键/剪贴板的复制粘贴逻辑
        }

        // 渲染
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (background)
            SDL_RenderCopy(renderer, background, nullptr, nullptr);

        // 计算显示范围
        int log_area_height = h - 2 * PADDING - LINE_HEIGHT;
        int maxVisible = std::max(1, log_area_height / LINE_HEIGHT);
        int total = (int)logLines.size();

        float maxScroll = std::max(0.0f, (float)(total - maxVisible) * LINE_HEIGHT);
        if (scrollOffset > maxScroll)
            scrollOffset = maxScroll;

        int firstLine = total - maxVisible - (int)(scrollOffset / LINE_HEIGHT);
        if (firstLine < 0)
            firstLine = 0;

        // 绘制日志
        for (int i = 0; i < maxVisible + 1; ++i)
        {
            int idx = firstLine + i;
            if (idx >= total)
                break;
            auto &rl = logLines[idx];
            if (rl && rl->texture)
            {
                SDL_Rect dst{PADDING, PADDING + i * LINE_HEIGHT, rl->width, rl->height};
                SDL_RenderCopy(renderer, rl->texture, nullptr, &dst);
            }
        }

        // 绘制输入行（路径 + 提示）
        std::string prompt_text = fs.getCurrentPath() + " $ ";
        auto promptRl = render_text(renderer, font, prompt_text); // 白字
        auto inputRl = render_text(renderer, font, input_text);   // 白字

        int input_y = h - PADDING - LINE_HEIGHT;
        if (promptRl && promptRl->texture)
        {
            SDL_Rect dst{PADDING, input_y, promptRl->width, promptRl->height};
            SDL_RenderCopy(renderer, promptRl->texture, nullptr, &dst);
        }
        if (inputRl && inputRl->texture)
        {
            SDL_Rect dst{PADDING + (promptRl ? promptRl->width : 0), input_y, inputRl->width, inputRl->height};
            SDL_RenderCopy(renderer, inputRl->texture, nullptr, &dst);
        }

        // 光标
        if ((SDL_GetTicks() / 500) % 2 == 0)
        {
            int cx = PADDING + (promptRl ? promptRl->width : 0) + (inputRl ? inputRl->width : 0);
            SDL_Rect cursor{cx, input_y + 3, 2, FONT_SIZE};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &cursor);
        }

        SDL_RenderPresent(renderer);
    }

    // 清理
    SDL_StopTextInput();
    TTF_CloseFont(font);
    if (background)
        SDL_DestroyTexture(background);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}