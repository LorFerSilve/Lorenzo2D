#pragma once

#include <SFML/System/Vector2.hpp>

namespace l2d
{
    class Camera2D;

    class OrthographicCameraController2D
    {
      public:
        explicit OrthographicCameraController2D(Camera2D& camera);
        OrthographicCameraController2D(const OrthographicCameraController2D&) = delete;
        OrthographicCameraController2D& operator=(const OrthographicCameraController2D&) = delete;
        OrthographicCameraController2D(OrthographicCameraController2D&&) = delete;
        OrthographicCameraController2D& operator=(OrthographicCameraController2D&&) = delete;

        void setFollowEnabled(bool enabled);
        bool isFollowEnabled() const;

        void setFollowTarget(sf::Vector2f target);
        void clearFollowTarget();
        bool hasFollowTarget() const;

        void setZoomEnabled(bool enabled);
        bool isZoomEnabled() const;

        // The current zoom is immediately clamped into the sanitized range.
        void setZoomLimits(float minZoom, float maxZoom);
        // Reported limits are capped to what the camera's current size can
        // represent safely.
        float minZoom() const;
        float maxZoom() const;

        // Nonfinite or nonpositive factors use the default values. Positive
        // factors retain their historic semantics, including inverted zoom.
        void setZoomStepFactors(float zoomInFactor, float zoomOutFactor);
        float zoomInFactor() const;
        float zoomOutFactor() const;

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
