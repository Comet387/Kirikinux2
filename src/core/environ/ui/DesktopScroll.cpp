// SPDX-License-Identifier: AGPL-3.0-only
#include "DesktopScroll.h"
#include "cocos2d/MainScene.h"
#include "cocos2d.h"
#include "ui/UIScrollView.h"
#include "extensions/GUI/CCScrollView/CCScrollView.h"
#include <algorithm>
#include <functional>
using namespace cocos2d;
#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
namespace {
// One controller per viewport, outside its moving content. Native game layers
// are never passed here: their wheel events continue to reach TJS unchanged.
class DesktopScroll : public Node {
    Node *view;
    std::function<Size()> viewport, content;
    std::function<float()> offset;
    std::function<void(float)> setOffset;
    DrawNode *bar;
    EventListenerTouchOneByOne *barTouch=nullptr;
    Rect track, thumb;
    float range = 0, grab = 0;
    bool dragging = false;
    bool available() const {
        for (Node *n = view; n; n = n->getParent()) if (!n->isVisible()) return false;
        return view->isRunning() && range > 0 && TVPMainScene::GetInstance()->isTopUI(view);
    }
    bool hit(const Vec2 &world) const { return Rect(Vec2::ZERO, viewport()).containsPoint(view->convertToNodeSpace(world)); }
    void moveThumb(float y) {
        float travel = track.size.height - thumb.size.height;
        if (travel > 0) setOffset(-range * std::max(0.f, std::min(1.f, (y - grab - track.origin.y) / travel)));
    }
public:
    ~DesktopScroll() override { if(barTouch)getEventDispatcher()->removeEventListener(barTouch); }
    static void attach(Node *v, std::function<Size()> vp, std::function<Size()> ct,
                       std::function<float()> off, std::function<void(float)> put) {
        if (v->Node::getChildByName("kirikinuxDesktopScroll")) return;
        auto *n = new DesktopScroll;
        n->init(); n->autorelease(); n->view=v; n->viewport=vp; n->content=ct; n->offset=off; n->setOffset=put;
        n->setName("kirikinuxDesktopScroll");
        n->bar=DrawNode::create(); n->addChild(n->bar);
        v->Node::addChild(n, 10000, n->getName()); // bypass ScrollView::addChild's content routing
        auto *mouse=EventListenerMouse::create();
        mouse->onMouseScroll=[n](Event *event) {
            auto *e=static_cast<EventMouse*>(event);
            if (!n->available() || !n->hit(e->getLocation())) return;
            float pixels = std::max(0.001f, n->view->convertToWorldSpace(Vec2(0,1)).distance(n->view->convertToWorldSpace(Vec2::ZERO)) * Director::getInstance()->getOpenGLView()->getScaleY());
            n->setOffset(std::max(-n->range, std::min(0.f, n->offset() + e->getScrollY()*48.f/pixels)));
            event->stopPropagation();
        };
        n->getEventDispatcher()->addEventListenerWithSceneGraphPriority(mouse,v);
        auto *touch=EventListenerTouchOneByOne::create(); touch->setSwallowTouches(true);
        touch->onTouchBegan=[n](Touch *t, Event*) {
            if (!n->available()) return false;
            Vec2 p=n->view->convertToNodeSpace(t->getLocation());
            if (!n->track.containsPoint(p)) return false;
            n->grab=n->thumb.containsPoint(p) ? p.y-n->thumb.origin.y : n->thumb.size.height/2;
            n->dragging=true; n->moveThumb(p.y); return true;
        };
        touch->onTouchMoved=[n](Touch *t,Event*) { if(n->dragging)n->moveThumb(n->view->convertToNodeSpace(t->getLocation()).y); };
        touch->onTouchEnded=[n](Touch*,Event*) { n->dragging=false; };
        touch->onTouchCancelled=touch->onTouchEnded;
        n->barTouch=touch;
        static int priority=-1000;
        n->getEventDispatcher()->addEventListenerWithFixedPriority(touch,--priority);
        n->scheduleUpdate();
    }
    void update(float) override {
        Size size=viewport(), inner=content(); range=std::max(0.f,inner.height-size.height);
        bar->clear(); if (range <= 0) return;
        float unit=std::max(0.001f,view->convertToWorldSpace(Vec2(1,0)).distance(view->convertToWorldSpace(Vec2::ZERO)) * Director::getInstance()->getOpenGLView()->getScaleX());
        float width=12.f/unit, margin=2.f/unit;
        track=Rect(std::max(0.f,size.width-width-margin), margin, width, std::max(0.f,size.height-2*margin));
        float h=std::min(track.size.height,std::max(28.f/unit,track.size.height*size.height/inner.height));
        float ratio=std::max(0.f,std::min(1.f,-offset()/range));
        thumb=Rect(track.origin.x,track.origin.y+ratio*(track.size.height-h),width,h);
        bar->drawSolidRect(track.origin,track.origin+Vec2(track.size),Color4F(.18f,.20f,.19f,.95f));
        bar->drawSolidRect(thumb.origin,thumb.origin+Vec2(thumb.size),Color4F(.65f,.70f,.66f,1.f));
    }
};
}
#endif
void TVPEnableDesktopScroll(ui::ScrollView *v) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
    v->setScrollBarEnabled(false);
    DesktopScroll::attach(v,[v]{return v->getContentSize();},[v]{return v->getInnerContainerSize();},
        [v]{return v->getInnerContainerPosition().y;},[v](float y){v->stopAutoScroll();Vec2 p=v->getInnerContainerPosition();p.y=y;v->setInnerContainerPosition(p);});
#endif
}
void TVPEnableDesktopScroll(extension::ScrollView *v) {
#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
    DesktopScroll::attach(v,[v]{return v->getViewSize();},[v]{return v->getContainer()->getContentSize();},
        [v]{return v->getContentOffset().y;},[v](float y){Vec2 p=v->getContentOffset();p.y=y;v->setContentOffset(p);});
#endif
}
