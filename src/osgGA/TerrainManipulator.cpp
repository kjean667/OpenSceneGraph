#include <osgGA/TerrainManipulator>
#include <osg/Quat>
#include <osg/Notify>
#include <osgUtil/IntersectVisitor>

using namespace osg;
using namespace osgGA;

TerrainManipulator::TerrainManipulator()
{
    _modelScale = 0.01f;
    _minimumZoomScale = 0.0005f;
    _thrown = false;

    _distance = 1.0f;
}


TerrainManipulator::~TerrainManipulator()
{
}


void TerrainManipulator::setNode(osg::Node* node)
{
    _node = node;
    if (_node.get())
    {
        const osg::BoundingSphere& boundingSphere=_node->getBound();
        _modelScale = boundingSphere._radius;
    }
}


const osg::Node* TerrainManipulator::getNode() const
{
    return _node.get();
}


osg::Node* TerrainManipulator::getNode()
{
    return _node.get();
}


void TerrainManipulator::home(const GUIEventAdapter& ,GUIActionAdapter& us)
{
    if(_node.get())
    {

        const osg::BoundingSphere& boundingSphere=_node->getBound();

        computePosition(boundingSphere._center+osg::Vec3( 0.0,-3.5f * boundingSphere._radius,0.0f),
                        boundingSphere._center,
                        osg::Vec3(0.0f,0.0f,1.0f));

        us.requestRedraw();
    }

}


void TerrainManipulator::init(const GUIEventAdapter& ,GUIActionAdapter& )
{
    flushMouseEventStack();
}


void TerrainManipulator::getUsage(osg::ApplicationUsage& usage) const
{
    usage.addKeyboardMouseBinding("Trackball: Space","Reset the viewing position to home");
    usage.addKeyboardMouseBinding("Trackball: +","When in stereo, increase the fusion distance");
    usage.addKeyboardMouseBinding("Trackball: -","When in stereo, reduse the fusion distance");
}

bool TerrainManipulator::handle(const GUIEventAdapter& ea,GUIActionAdapter& us)
{
    switch(ea.getEventType())
    {
        case(GUIEventAdapter::PUSH):
        {
            flushMouseEventStack();
            addMouseEvent(ea);
            if (calcMovement()) us.requestRedraw();
            us.requestContinuousUpdate(false);
            _thrown = false;
            return true;
        }

        case(GUIEventAdapter::RELEASE):
        {
            if (ea.getButtonMask()==0)
            {

                if (isMouseMoving())
                {
                    if (calcMovement())
                    {
                        us.requestRedraw();
                        us.requestContinuousUpdate(true);
                        _thrown = true;
                    }
                }
                else
                {
                    flushMouseEventStack();
                    addMouseEvent(ea);
                    if (calcMovement()) us.requestRedraw();
                    us.requestContinuousUpdate(false);
                    _thrown = false;
                }

            }
            else
            {
                flushMouseEventStack();
                addMouseEvent(ea);
                if (calcMovement()) us.requestRedraw();
                us.requestContinuousUpdate(false);
                _thrown = false;
            }
            return true;
        }

        case(GUIEventAdapter::DRAG):
        {
            addMouseEvent(ea);
            if (calcMovement()) us.requestRedraw();
            us.requestContinuousUpdate(false);
            _thrown = false;
            return true;
        }

        case(GUIEventAdapter::MOVE):
        {
            return false;
        }

        case(GUIEventAdapter::KEYDOWN):
            if (ea.getKey()==' ')
            {
                flushMouseEventStack();
                _thrown = false;
                home(ea,us);
                us.requestRedraw();
                us.requestContinuousUpdate(false);
                return true;
            }
            return false;
        case(GUIEventAdapter::FRAME):
            if (_thrown)
            {
                if (calcMovement()) us.requestRedraw();
            }
            return false;
        default:
            return false;
    }
}


bool TerrainManipulator::isMouseMoving()
{
    if (_ga_t0.get()==NULL || _ga_t1.get()==NULL) return false;

    static const float velocity = 0.1f;

    float dx = _ga_t0->getXnormalized()-_ga_t1->getXnormalized();
    float dy = _ga_t0->getYnormalized()-_ga_t1->getYnormalized();
    float len = sqrtf(dx*dx+dy*dy);
    float dt = _ga_t0->time()-_ga_t1->time();

    return (len>dt*velocity);
}


void TerrainManipulator::flushMouseEventStack()
{
    _ga_t1 = NULL;
    _ga_t0 = NULL;
}


void TerrainManipulator::addMouseEvent(const GUIEventAdapter& ea)
{
    _ga_t1 = _ga_t0;
    _ga_t0 = &ea;
}

void TerrainManipulator::setByMatrix(const osg::Matrixd& matrix)
{
    osg::Vec3 lookVector(- matrix(2,0),-matrix(2,1),-matrix(2,2));
    osg::Vec3 eye(matrix(3,0),matrix(3,1),matrix(3,2));
    
    osg::notify(INFO)<<"eye point "<<eye<<std::endl;
    osg::notify(INFO)<<"lookVector "<<lookVector<<std::endl;

    // need to reintersect with the terrain
    osgUtil::IntersectVisitor iv;

    const osg::BoundingSphere& bs = _node->getBound();
    float distance = (eye-bs.center()).length() + _node->getBound().radius();
    osg::Vec3 start_segment = eye;
    osg::Vec3 end_segment = eye + lookVector*distance;

    osg::notify(INFO)<<"start="<<start_segment<<"\tend="<<end_segment<<"\tupVector="<<getUpVector(_coordinateFrame)<<std::endl;

    osg::ref_ptr<osg::LineSegment> segLookVector = new osg::LineSegment;
    segLookVector->set(start_segment,end_segment);
    iv.addLineSegment(segLookVector.get());

    _node->accept(iv);

    bool hitFound = false;
    if (iv.hits())
    {
        osgUtil::IntersectVisitor::HitList& hitList = iv.getHitList(segLookVector.get());
        if (!hitList.empty())
        {
            notify(INFO) << "Hit terrain ok"<< std::endl;
            osg::Vec3 ip = hitList.front().getWorldIntersectPoint();
            _coordinateFrame = getCoordinateFrame( ip.x(), ip.y(), ip.z());

            _distance = (eye-ip).length();
            
            osg::Matrix rotation_matrix = osg::Matrixd::translate(0.0,0.0,-_distance)*
                                          matrix*
                                          osg::Matrixd::inverse(_coordinateFrame);

            rotation_matrix.get(_rotation);

            hitFound = true;
        }
    }

    if (!hitFound)
    {
        CoordinateFrame eyePointCoordFrame = getCoordinateFrame( eye.x(), eye.y(), eye.z());
        
        // clear the intersect visitor ready for a new test
        iv.reset(); 
               
        osg::ref_ptr<osg::LineSegment> segDowVector = new osg::LineSegment;
        segLookVector->set(eye+getUpVector(eyePointCoordFrame)*distance,
                           eye-getUpVector(eyePointCoordFrame)*distance);
        iv.addLineSegment(segLookVector.get());

        _node->accept(iv);
        
        hitFound = false;
        if (iv.hits())
        {
            osgUtil::IntersectVisitor::HitList& hitList = iv.getHitList(segLookVector.get());
            if (!hitList.empty())
            {
                notify(INFO) << "Hit terrain ok"<< std::endl;
                osg::Vec3 ip = hitList.front().getWorldIntersectPoint();

                _coordinateFrame = getCoordinateFrame( ip.x(), ip.y(), ip.z());

                _distance = (eye-ip).length();

                _rotation.set(0,0,0,1);

                hitFound = true;
            }
        }
    }    

}

osg::Matrixd TerrainManipulator::getMatrix() const
{
    return osg::Matrixd::translate(0.0,0.0,_distance)*osg::Matrixd::rotate(_rotation)*_coordinateFrame;
}

osg::Matrixd TerrainManipulator::getInverseMatrix() const
{
    return osg::Matrixd::inverse(_coordinateFrame)*osg::Matrixd::rotate(_rotation.inverse())*osg::Matrixd::translate(0.0,0.0,-_distance);
}

void TerrainManipulator::computePosition(const osg::Vec3& eye,const osg::Vec3& center,const osg::Vec3& up)
{
    // compute rotation matrix
    osg::Vec3 lv(center-eye);
    _distance = lv.length();

    // compute the itersection with the scene.
    osgUtil::IntersectVisitor iv;

    osg::ref_ptr<osg::LineSegment> segLookVector = new osg::LineSegment;
    segLookVector->set(eye,center);
    iv.addLineSegment(segLookVector.get());

    _node->accept(iv);

    bool hitFound = false;
    if (iv.hits())
    {
        osgUtil::IntersectVisitor::HitList& hitList = iv.getHitList(segLookVector.get());
        if (!hitList.empty())
        {
            osg::notify(osg::INFO) << "Hit terrain ok"<< std::endl;
            osg::Vec3 ip = hitList.front().getWorldIntersectPoint();
            osg::Vec3 np = hitList.front().getWorldIntersectNormal();

            _coordinateFrame = getCoordinateFrame( ip.x(), ip.y(), ip.z());
            _distance = (ip-eye).length();
            
            hitFound = true;
        }
    }
    
    if (!hitFound)
    {
        // ??
        _coordinateFrame = getCoordinateFrame( center.x(), center.y(), center.z());
    }


    // note LookAt = inv(CF)*inv(RM)*inv(T) which is equivilant to:
    // inv(R) = CF*LookAt.

    osg::Matrixd rotation_matrix = _coordinateFrame*osg::Matrixd::lookAt(eye,center,up);

    rotation_matrix.get(_rotation);
    _rotation = _rotation.inverse();
}


bool TerrainManipulator::calcMovement()
{
    // return if less then two events have been added.
    if (_ga_t0.get()==NULL || _ga_t1.get()==NULL) return false;

    float dx = _ga_t0->getXnormalized()-_ga_t1->getXnormalized();
    float dy = _ga_t0->getYnormalized()-_ga_t1->getYnormalized();


    // return if there is no movement.
    if (dx==0 && dy==0) return false;

    unsigned int buttonMask = _ga_t1->getButtonMask();
    if (buttonMask==GUIEventAdapter::LEFT_MOUSE_BUTTON)
    {

        // rotate camera.

        osg::Vec3 axis;
        float angle;

        float px0 = _ga_t0->getXnormalized();
        float py0 = _ga_t0->getYnormalized();
        
        float px1 = _ga_t1->getXnormalized();
        float py1 = _ga_t1->getYnormalized();
        

        trackball(axis,angle,px1,py1,px0,py0);

        osg::Quat new_rotate;
        new_rotate.makeRotate(angle,axis);
        
        _rotation = _rotation*new_rotate;

        return true;

    }
    else if (buttonMask==GUIEventAdapter::MIDDLE_MOUSE_BUTTON ||
        buttonMask==(GUIEventAdapter::LEFT_MOUSE_BUTTON|GUIEventAdapter::RIGHT_MOUSE_BUTTON))
    {

        // pan model.

        float scale = -0.5f*_distance;

        osg::Matrix rotation_matrix;
        rotation_matrix.set(_rotation);

        osg::Vec3 dv(dx*scale,dy*scale,0.0f);

        // _center += dv*rotation_matrix;

        // need to recompute the itersection point along the look vector.
        
        _coordinateFrame = osg::Matrixd::rotate(_rotation.inverse())*
                           osg::Matrixd::translate(dx*scale,dy*scale,0.0f)*
                           osg::Matrixd::rotate(_rotation)*
                           _coordinateFrame;

        // osg::notify(osg::INDFO)<<"\tafter "<<_coordinateFrame.getTrans()<<std::endl;

        // now reorientate the coordinate frame to the frame coords.
        _coordinateFrame = getCoordinateFrame( _coordinateFrame(3,0), _coordinateFrame(3,1), _coordinateFrame(3,2));

        // need to reintersect with the terrain
        osgUtil::IntersectVisitor iv;

        float distance = _node->getBound().radius();
        osg::Vec3 start_segment = _coordinateFrame.getTrans() + getUpVector(_coordinateFrame) * distance;
        osg::Vec3 end_segment = start_segment - getUpVector(_coordinateFrame) * (2.0f*distance);
        //end_segment.set(0.0f,0.0f,0.0f);

        osg::notify(INFO)<<"start="<<start_segment<<"\tend="<<end_segment<<"\tupVector="<<getUpVector(_coordinateFrame)<<std::endl;

        osg::ref_ptr<osg::LineSegment> segLookVector = new osg::LineSegment;
        segLookVector->set(start_segment,end_segment);
        iv.addLineSegment(segLookVector.get());

        _node->accept(iv);

        bool hitFound = false;
        if (iv.hits())
        {
            osgUtil::IntersectVisitor::HitList& hitList = iv.getHitList(segLookVector.get());
            if (!hitList.empty())
            {
                notify(INFO) << "Hit terrain ok"<< std::endl;
                osg::Vec3 ip = hitList.front().getWorldIntersectPoint();
                _coordinateFrame = getCoordinateFrame( ip.x(), ip.y(), ip.z());

                hitFound = true;
            }
        }

        if (!hitFound)
        {
            // ??
            osg::notify(INFO)<<"TerrainManipulator unable to intersect with terrain."<<std::endl;
        }
        
        return true;

    }
    else if (buttonMask==GUIEventAdapter::RIGHT_MOUSE_BUTTON)
    {

        // zoom model.

        float fd = _distance;
        float scale = 1.0f+dy;
        if (fd*scale>_modelScale*_minimumZoomScale)
        {

            _distance *= scale;

        }
        
        return true;

    }

    return false;
}


/*
 * This size should really be based on the distance from the center of
 * rotation to the point on the object underneath the mouse.  That
 * point would then track the mouse as closely as possible.  This is a
 * simple example, though, so that is left as an Exercise for the
 * Programmer.
 */
const float TRACKBALLSIZE = 0.8f;

/*
 * Ok, simulate a track-ball.  Project the points onto the virtual
 * trackball, then figure out the axis of rotation, which is the cross
 * product of P1 P2 and O P1 (O is the center of the ball, 0,0,0)
 * Note:  This is a deformed trackball-- is a trackball in the center,
 * but is deformed into a hyperbolic sheet of rotation away from the
 * center.  This particular function was chosen after trying out
 * several variations.
 *
 * It is assumed that the arguments to this routine are in the range
 * (-1.0 ... 1.0)
 */
void TerrainManipulator::trackball(osg::Vec3& axis,float& angle, float p1x, float p1y, float p2x, float p2y)
{
    /*
     * First, figure out z-coordinates for projection of P1 and P2 to
     * deformed sphere
     */

    osg::Matrix rotation_matrix(_rotation);


    osg::Vec3 uv = osg::Vec3(0.0f,1.0f,0.0f)*rotation_matrix;
    osg::Vec3 sv = osg::Vec3(1.0f,0.0f,0.0f)*rotation_matrix;
    osg::Vec3 lv = osg::Vec3(0.0f,0.0f,-1.0f)*rotation_matrix;

    osg::Vec3 p1 = sv*p1x+uv*p1y-lv*tb_project_to_sphere(TRACKBALLSIZE,p1x,p1y);
    osg::Vec3 p2 = sv*p2x+uv*p2y-lv*tb_project_to_sphere(TRACKBALLSIZE,p2x,p2y);

    /*
     *  Now, we want the cross product of P1 and P2
     */

// Robert,
//
// This was the quick 'n' dirty  fix to get the trackball doing the right 
// thing after fixing the Quat rotations to be right-handed.  You may want
// to do something more elegant.
//   axis = p1^p2;
axis = p2^p1;
    axis.normalize();

    /*
     *  Figure out how much to rotate around that axis.
     */
    float t = (p2-p1).length() / (2.0*TRACKBALLSIZE);

    /*
     * Avoid problems with out-of-control values...
     */
    if (t > 1.0) t = 1.0;
    if (t < -1.0) t = -1.0;
    angle = inRadians(asin(t));

}


/*
 * Project an x,y pair onto a sphere of radius r OR a hyperbolic sheet
 * if we are away from the center of the sphere.
 */
float TerrainManipulator::tb_project_to_sphere(float r, float x, float y)
{
    float d, t, z;

    d = sqrt(x*x + y*y);
                                 /* Inside sphere */
    if (d < r * 0.70710678118654752440)
    {
        z = sqrt(r*r - d*d);
    }                            /* On hyperbola */
    else
    {
        t = r / 1.41421356237309504880;
        z = t*t / d;
    }
    return z;
}
