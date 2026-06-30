#pragma once

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class Camera2D;

    class OrthographicCameraController2D
    {
    public:
        explicit OrthographicCameraController2D(Camera2D& camera);

        void setFollowEnabled(bool enabled);
        bool isFollowEnabled() const;

        void setFollowTarget(sf::Vector2f target);
        void clearFollowTarget();
        bool hasFollowTarget() const;

        void setZoomEnabled(bool enabled);
        bool isZoomEnabled() const;

        void setZoomLimits(float minZoom, float maxZoom);
        float minZoom() const;
        float maxZoom() const;

        void setZoomStepFactors(float zoomInFactor, float zoomOutFactor);

        void setResizeEnabled(bool enabled);
        bool isResizeEnabled() const;

        void update(float deltaTime);

    private:
        void updateResize();
        void updateFollow(float deltaTime);
        void updateZoom();

    private:
        Camera2D* m_camera;

        bool m_followEnabled;
        bool m_hasFollowTarget;
        sf::Vector2f m_followTarget;

        bool m_zoomEnabled;
        float m_minZoom;
        float m_maxZoom;
        float m_zoomInFactor;
        float m_zoomOutFactor;

        bool m_resizeEnabled;
    };
}