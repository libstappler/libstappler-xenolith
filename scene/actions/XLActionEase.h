/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef XENOLITH_SCENE_ACTIONS_XLACTIONEASE_H_
#define XENOLITH_SCENE_ACTIONS_XLACTIONEASE_H_

#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::interpolation {

enum Type {
	Linear,

	EaseIn,
	EaseOut,
	EaseInOut,

	Sine_EaseIn,
	Sine_EaseOut,
	Sine_EaseInOut,

	Quad_EaseIn,
	Quad_EaseOut,
	Quad_EaseInOut,

	Cubic_EaseIn,
	Cubic_EaseOut,
	Cubic_EaseInOut,

	Quart_EaseIn,
	Quart_EaseOut,
	Quart_EaseInOut,

	Quint_EaseIn,
	Quint_EaseOut,
	Quint_EaseInOut,

	Expo_EaseIn,
	Expo_EaseOut,
	Expo_EaseInOut,

	Circ_EaseIn,
	Circ_EaseOut,
	Circ_EaseInOut,

	Elastic_EaseIn,
	Elastic_EaseOut,
	Elastic_EaseInOut,

	Back_EaseIn,
	Back_EaseOut,
	Back_EaseInOut,

	Bounce_EaseIn,
	Bounce_EaseOut,
	Bounce_EaseInOut,

	Custom,

	Max
};

SP_PUBLIC float interpolateTo(float time, Type type, float *easingParam);

SP_PUBLIC float linear(float time);

SP_PUBLIC float easeIn(float time, float rate);
SP_PUBLIC float easeOut(float time, float rate);
SP_PUBLIC float easeInOut(float time, float rate);

SP_PUBLIC float bezieratFunction(float t, float a, float b, float c, float d);

SP_PUBLIC float quadraticIn(float time);
SP_PUBLIC float quadraticOut(float time);
SP_PUBLIC float quadraticInOut(float time);

SP_PUBLIC float sineEaseIn(float time);
SP_PUBLIC float sineEaseOut(float time);
SP_PUBLIC float sineEaseInOut(float time);

SP_PUBLIC float quadEaseIn(float time);
SP_PUBLIC float quadEaseOut(float time);
SP_PUBLIC float quadEaseInOut(float time);

SP_PUBLIC float cubicEaseIn(float time);
SP_PUBLIC float cubicEaseOut(float time);
SP_PUBLIC float cubicEaseInOut(float time);

SP_PUBLIC float quartEaseIn(float time);
SP_PUBLIC float quartEaseOut(float time);
SP_PUBLIC float quartEaseInOut(float time);

SP_PUBLIC float quintEaseIn(float time);
SP_PUBLIC float quintEaseOut(float time);
SP_PUBLIC float quintEaseInOut(float time);

SP_PUBLIC float expoEaseIn(float time);
SP_PUBLIC float expoEaseOut(float time);
SP_PUBLIC float expoEaseInOut(float time);

SP_PUBLIC float circEaseIn(float time);
SP_PUBLIC float circEaseOut(float time);
SP_PUBLIC float circEaseInOut(float time);

SP_PUBLIC float elasticEaseIn(float time, float period);
SP_PUBLIC float elasticEaseOut(float time, float period);
SP_PUBLIC float elasticEaseInOut(float time, float period);

SP_PUBLIC float backEaseIn(float time);
SP_PUBLIC float backEaseOut(float time);
SP_PUBLIC float backEaseInOut(float time);

SP_PUBLIC float bounceEaseIn(float time);
SP_PUBLIC float bounceEaseOut(float time);
SP_PUBLIC float bounceEaseInOut(float time);

SP_PUBLIC float customEase(float time, float *easingParam);

}

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class SP_PUBLIC ActionEase : public ActionInterval {
public:
	virtual ~ActionEase();

	bool init(ActionInterval *action);

	virtual void startWithTarget(Node *target) override;
	virtual void stop() override;
	virtual void update(float time) override;

protected:
	Rc<ActionInterval> _inner;
};

class SP_PUBLIC EaseRateAction : public ActionEase {
public:
	virtual ~EaseRateAction();

	bool init(ActionInterval *pAction, float fRate);

	inline void setRate(float rate) { _rate = rate; }
	inline float getRate() const { return _rate; }

protected:
	float _rate;
};

/**
 @class EaseIn
 @brief EaseIn action with a rate.
 @details The timeline of inner action will be changed by:
         \f${ time }^{ rate }\f$.
 @ingroup Actions
 */
class SP_PUBLIC EaseIn : public EaseRateAction {
public:
	SP_COVERAGE_TRIVIAL
	virtual ~EaseIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseOut
 @brief EaseOut action with a rate.
 @details The timeline of inner action will be changed by:
         \f${ time }^ { (1/rate) }\f$.
 @ingroup Actions
 */
class SP_PUBLIC EaseOut: public EaseRateAction {
public:
	SP_COVERAGE_TRIVIAL
	virtual ~EaseOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseInOut
 @brief EaseInOut action with a rate
 @details If time * 2 < 1, the timeline of inner action will be changed by:
		 \f$0.5*{ time }^{ rate }\f$.
		 Else, the timeline of inner action will be changed by:
		 \f$1.0-0.5*{ 2-time }^{ rate }\f$.
 @ingroup Actions
 */
class SP_PUBLIC EaseInOut: public EaseRateAction {
public:
	SP_COVERAGE_TRIVIAL
	virtual ~EaseInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseExponentialIn
 @brief Ease Exponential In action.
 @details The timeline of inner action will be changed by:
		 \f${ 2 }^{ 10*(time-1) }-1*0.001\f$.
 @ingroup Actions
 */
class SP_PUBLIC EaseExponentialIn: public ActionEase {
public:
	virtual ~EaseExponentialIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseExponentialOut
 @brief Ease Exponential Out
 @details The timeline of inner action will be changed by:
		 \f$1-{ 2 }^{ -10*(time-1) }\f$.
 @ingroup Actions
 */
class SP_PUBLIC EaseExponentialOut : public ActionEase {
public:
	virtual ~EaseExponentialOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseExponentialInOut
 @brief Ease Exponential InOut
 @details If time * 2 < 1, the timeline of inner action will be changed by:
		 \f$0.5*{ 2 }^{ 10*(time-1) }\f$.
		 else, the timeline of inner action will be changed by:
		 \f$0.5*(2-{ 2 }^{ -10*(time-1) })\f$.
 @ingroup Actions
 */
class SP_PUBLIC EaseExponentialInOut : public ActionEase {
public:
	virtual ~EaseExponentialInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseSineIn
 @brief Ease Sine In
 @details The timeline of inner action will be changed by:
		 \f$1-cos(time*\frac { \pi  }{ 2 } )\f$.
 @ingroup Actions
 */
class SP_PUBLIC EaseSineIn : public ActionEase {
public:
	virtual ~EaseSineIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseSineOut
 @brief Ease Sine Out
 @details The timeline of inner action will be changed by:
		 \f$sin(time*\frac { \pi  }{ 2 } )\f$.
 @ingroup Actions
 */
class SP_PUBLIC EaseSineOut : public ActionEase {
public:
	virtual ~EaseSineOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseSineInOut
 @brief Ease Sine InOut
 @details The timeline of inner action will be changed by:
		 \f$-0.5*(cos(\pi *time)-1)\f$.
 @ingroup Actions
 */
class SP_PUBLIC EaseSineInOut : public ActionEase {
public:
	virtual ~EaseSineInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseElastic
 @brief Ease Elastic abstract class
 @since v0.8.2
 @ingroup Actions
 */
class SP_PUBLIC EaseElastic : public ActionEase {
public:
	SP_COVERAGE_TRIVIAL
	virtual ~EaseElastic() { }

	bool init(ActionInterval *action, float period = 0.3f);

	inline float getPeriod() const { return _period; }
	inline void setPeriod(float fPeriod) { _period = fPeriod; }

protected:
	float _period = 0.3f;
};

/**
 @class EaseElasticIn
 @brief Ease Elastic In action.
 @details If time == 0 or time == 1, the timeline of inner action will not be changed.
		 Else, the timeline of inner action will be changed by:
		 \f$-{ 2 }^{ 10*(time-1) }*sin((time-1-\frac { period }{ 4 } )*\pi *2/period)\f$.

 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class SP_PUBLIC EaseElasticIn : public EaseElastic {
public:
	virtual ~EaseElasticIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseElasticOut
 @brief Ease Elastic Out action.
 @details If time == 0 or time == 1, the timeline of inner action will not be changed.
		 Else, the timeline of inner action will be changed by:
		 \f${ 2 }^{ -10*time }*sin((time-\frac { period }{ 4 } )*\pi *2/period)+1\f$.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class SP_PUBLIC EaseElasticOut : public EaseElastic {
public:
	virtual ~EaseElasticOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseElasticInOut
 @brief Ease Elastic InOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class SP_PUBLIC EaseElasticInOut : public EaseElastic {
public:
	virtual ~EaseElasticInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseBounceIn
 @brief EaseBounceIn action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
*/
class SP_PUBLIC EaseBounceIn : public ActionEase {
public:
	virtual ~EaseBounceIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseBounceOut
 @brief EaseBounceOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class SP_PUBLIC EaseBounceOut : public ActionEase {
public:
	virtual ~EaseBounceOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseBounceInOut
 @brief EaseBounceInOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class SP_PUBLIC EaseBounceInOut : public ActionEase {
public:
	virtual ~EaseBounceInOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseBackIn
 @brief EaseBackIn action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class SP_PUBLIC EaseBackIn : public ActionEase {
public:
	virtual ~EaseBackIn() { }

	virtual void update(float time) override;
};

/**
 @class EaseBackOut
 @brief EaseBackOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class SP_PUBLIC EaseBackOut : public ActionEase {
public:
	virtual ~EaseBackOut() { }

	virtual void update(float time) override;
};

/**
 @class EaseBackInOut
 @brief EaseBackInOut action.
 @warning This action doesn't use a bijective function.
		  Actions like Sequence might have an unexpected result when used with this action.
 @since v0.8.2
 @ingroup Actions
 */
class SP_PUBLIC EaseBackInOut : public ActionEase {
public:
	virtual ~EaseBackInOut() { }

	virtual void update(float time) override;
};

/**
@class EaseBezierAction
@brief Ease Bezier
@ingroup Actions
*/
class SP_PUBLIC EaseBezierAction : public ActionEase {
public:
	virtual ~EaseBezierAction() { }

	bool init(ActionInterval *, float p0, float p1, float p2, float p3);

	virtual void update(float time) override;

protected:
	float _p0;
	float _p1;
	float _p2;
	float _p3;
};

/**
@class EaseQuadraticActionIn
@brief Ease Quadratic In
@ingroup Actions
*/
class SP_PUBLIC EaseQuadraticActionIn : public ActionEase {
public:
	virtual ~EaseQuadraticActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseQuadraticActionOut
@brief Ease Quadratic Out
@ingroup Actions
*/
class SP_PUBLIC EaseQuadraticActionOut : public ActionEase {
public:
	virtual ~EaseQuadraticActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuadraticActionInOut
@brief Ease Quadratic InOut
@ingroup Actions
*/
class SP_PUBLIC EaseQuadraticActionInOut : public ActionEase {
public:
	virtual ~EaseQuadraticActionInOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuarticActionIn
@brief Ease Quartic In
@ingroup Actions
*/
class SP_PUBLIC EaseQuarticActionIn : public ActionEase {
public:
	virtual ~EaseQuarticActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseQuarticActionOut
@brief Ease Quartic Out
@ingroup Actions
*/
class SP_PUBLIC EaseQuarticActionOut : public ActionEase {
public:
	virtual ~EaseQuarticActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuarticActionInOut
@brief Ease Quartic InOut
@ingroup Actions
*/
class SP_PUBLIC EaseQuarticActionInOut : public ActionEase {
public:
	virtual ~EaseQuarticActionInOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuinticActionIn
@brief Ease Quintic In
@ingroup Actions
*/
class SP_PUBLIC EaseQuinticActionIn : public ActionEase {
public:
	virtual ~EaseQuinticActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseQuinticActionOut
@brief Ease Quintic Out
@ingroup Actions
*/
class SP_PUBLIC EaseQuinticActionOut : public ActionEase {
public:
	virtual ~EaseQuinticActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseQuinticActionInOut
@brief Ease Quintic InOut
@ingroup Actions
*/
class SP_PUBLIC EaseQuinticActionInOut : public ActionEase {
public:
	virtual ~EaseQuinticActionInOut() { }

	virtual void update(float time) override;
};

/**
@class EaseCircleActionIn
@brief Ease Circle In
@ingroup Actions
*/
class SP_PUBLIC EaseCircleActionIn : public ActionEase {
public:
	virtual ~EaseCircleActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseCircleActionOut
@brief Ease Circle Out
@ingroup Actions
*/
class SP_PUBLIC EaseCircleActionOut : public ActionEase {
public:
	virtual ~EaseCircleActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseCircleActionInOut
@brief Ease Circle InOut
@ingroup Actions
*/
class SP_PUBLIC EaseCircleActionInOut : public ActionEase {
public:
	virtual ~EaseCircleActionInOut() {}

	virtual void update(float time) override;
};

/**
@class EaseCubicActionIn
@brief Ease Cubic In
@ingroup Actions
*/
class SP_PUBLIC EaseCubicActionIn : public ActionEase {
public:
	virtual ~EaseCubicActionIn() { }

	virtual void update(float time) override;
};

/**
@class EaseCubicActionOut
@brief Ease Cubic Out
@ingroup Actions
*/
class SP_PUBLIC EaseCubicActionOut : public ActionEase {
public:
	virtual ~EaseCubicActionOut() { }

	virtual void update(float time) override;
};

/**
@class EaseCubicActionInOut
@brief Ease Cubic InOut
@ingroup Actions
*/
class SP_PUBLIC EaseCubicActionInOut : public ActionEase {
public:
	virtual ~EaseCubicActionInOut() { }

	virtual void update(float time) override;
};

}

#endif /* XENOLITH_SCENE_ACTIONS_XLACTIONEASE_H_ */
