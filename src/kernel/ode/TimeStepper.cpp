// Copyright (C) 2003 Johan Hoffman and Anders Logg.
// Licensed under the GNU GPL Version 2.
//
// Modified by Johan Jansson 2003, 2004.

#include <cmath>
#include <dolfin/dolfin_log.h>
#include <dolfin/dolfin_settings.h>
#include <dolfin/timeinfo.h>
#include <dolfin/ODE.h>
#include <dolfin/Sample.h>
#include <dolfin/TimeSlab.h>
#include <dolfin/SimpleTimeSlab.h>
#include <dolfin/RecursiveTimeSlab.h>
#include <dolfin/TimeStepper.h>

using namespace dolfin;

//-----------------------------------------------------------------------------
TimeStepper::TimeStepper(ODE& ode, Function& function) :
  no_samples(dolfin_get("number of samples")), N(ode.size()), t(0),
  T(ode.endtime()), partition(N), adaptivity(ode), u(ode, function), f(ode, u),
  fixpoint(u, f, adaptivity), file(u.label() + ".m"), p("Time-stepping"),
  _finished(false), save_solution(dolfin_get("save solution"))
{
  dolfin_warning("ODE solver is EXPERIMENTAL.");

  // Start timing
  tic();
}
//-----------------------------------------------------------------------------
TimeStepper::~TimeStepper()
{
   // Display status report
  cout << "Solution computed in " << toc() << " seconds." << endl;
  fixpoint.report();
}
//-----------------------------------------------------------------------------
void TimeStepper::solve(ODE& ode, Function& function)
{
  // Create a TimeStepper object
  TimeStepper timeStepper(ode, function);

  // Do time stepping
  while ( !timeStepper.finished() )
    timeStepper.step();
}
//-----------------------------------------------------------------------------
real TimeStepper::step()
{
  // Repeat until the time slab has converged
  while ( !createTimeSlab() );

  return t;
}
//-----------------------------------------------------------------------------
bool TimeStepper::finished() const
{
  return _finished;
}
//-----------------------------------------------------------------------------
bool TimeStepper::createTimeSlab()
{
  if ( t == 0.0 )
    return createFirstTimeSlab();
  else
    return createGeneralTimeSlab();
}
//-----------------------------------------------------------------------------
bool TimeStepper::createFirstTimeSlab()
{
  // Create the time slab
  SimpleTimeSlab timeslab(t, T, u, adaptivity);

  // Try to solve the system using fixed point iteration
  if ( !fixpoint.iterate(timeslab) )
  {
    stabilize(timeslab.length());
    u.reset();
    return false;
  }

  // Check if the residual is small enough if the time step is not fixed
  if ( !adaptivity.fixed() )
  {
    if ( !adaptivity.accept(timeslab, f) )
    {
      cout << "Residual is too large, creating a new time slab." << endl;
      adaptivity.shift(u, f);
      u.reset();
      return false;
    }
  }

  // Update time
  t = timeslab.endtime();
  
  // Save solution
  save(timeslab);
  
  // Prepare for next time slab
  shift();
  
  // Update progress
  p = t / T;

  // Check if we are done
  if ( timeslab.finished() )
  {
    _finished = true;
    p = 1.0;
  }

  return true;
}
//-----------------------------------------------------------------------------
bool TimeStepper::createGeneralTimeSlab()
{
  // Create the time slab
  RecursiveTimeSlab timeslab(t, T, u, f, adaptivity, fixpoint, partition, 0);

  // Try to solve the system using fixed point iteration
  if ( !fixpoint.iterate(timeslab) )
  {
    stabilize(timeslab.length());
    u.reset();
    return false;
  }
  
  /*
  // Check if the residual is small enough
  if ( !adaptivity.accept(timeslab, f) )
  {
    cout << "Residual is too large, creating a new time slab." << endl;
    adaptivity.shift(u, f);
    u.reset();
    return false;
  }
  */

  // Update time
  t = timeslab.endtime();
  
  // Save solution
  save(timeslab);
  
  // Prepare for next time slab
  shift();
  
  // Update progress
  p = t / T;

  // Check if we are done
  if ( timeslab.finished() )
  {
    _finished = true;
    p = 1.0;
  }

  return true;
}
//-----------------------------------------------------------------------------
void TimeStepper::shift()
{
  // Shift adaptivity
  adaptivity.shift(u, f);

  // Shift solution
  u.shift(t);
}
//-----------------------------------------------------------------------------
void TimeStepper::save(TimeSlab& timeslab)
{
  // Check if we should save the solution
  if ( !save_solution )
    return;

  // Compute time of first sample within time slab
  real K = T / static_cast<real>(no_samples);
  real t = ceil(timeslab.starttime()/K) * K;

  // Save samples
  while ( t < timeslab.endtime() )
  {
    Sample sample(u, f, t);
    file << sample;
    t += K;
  }
  
  // Save end time value
  if ( timeslab.finished() ) {
    Sample sample(u, f, timeslab.endtime());
    file << sample;
  }
}
//-----------------------------------------------------------------------------
void TimeStepper::stabilize(real K)
{
  // Get stabilization parameters from fixed point iteration
  real alpha = 1.0;
  unsigned int m = 0;
  fixpoint.stabilization(alpha, m);

  // Compute stabilizing time step, at least (at most) a factor 1/2
  real k = std::min(alpha, 0.5) * K;

  // Stabilize
  adaptivity.stabilize(k, m);
}
//-----------------------------------------------------------------------------
