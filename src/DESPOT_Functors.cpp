//
//  DESPOT_Functors.cpp
//  DESPOT
//
//  Created by Tobias Wood on 25/09/2013.
//
//

#include "DESPOT_Functors.h"

//******************************************************************************
#pragma mark Signal Functors
//******************************************************************************
SignalFunctor::SignalFunctor(const ArrayXd &flip, const double TR, const Components nC) :
	m_flip(flip), m_TR(TR), m_nC(nC) {}

SPGR_Functor::SPGR_Functor(const ArrayXd &flip, const double TR, const Components nC) :
	SignalFunctor(flip, TR, nC) {}
ArrayXd SPGR_Functor::signal(const VectorXd &p, const double B1, double f0) {
	switch (m_nC) {
		case (Components::One) : return SigMag(One_SPGR(p, m_flip, m_TR, B1));
		case (Components::Two) : return SigMag(Two_SPGR(p, m_flip, m_TR, B1));
		case (Components::Three) : return SigMag(Three_SPGR(p, m_flip, m_TR, B1));
	}
}

SSFP_Functor::SSFP_Functor(const ArrayXd &flip, const double TR, const ArrayXd &phases, const Components nC) :
	SignalFunctor(flip, TR, nC), m_phases(phases) {}

size_t SSFP_Functor::size() const {
	return m_flip.rows() * m_phases.rows();
}

ArrayXd SSFP_Functor::signal(const VectorXd &p, const double B1, double f0) {
	ArrayXd s(size());
	ArrayXd::Index start = 0;
	for (ArrayXd::Index i = 0; i < m_phases.rows(); i++) {
		switch (m_nC) {
			case (Components::One) : s.segment(start, m_flip.rows()) = SigMag(One_SSFP(p, m_flip, m_TR, m_phases(i), B1, f0)); break;
			case (Components::Two) : s.segment(start, m_flip.rows()) = SigMag(Two_SSFP(p, m_flip, m_TR, m_phases(i), B1, f0)); break;
			case (Components::Three) : s.segment(start, m_flip.rows()) = SigMag(Three_SSFP(p, m_flip, m_TR, m_phases(i), B1, f0)); break;
		}
		start += m_flip.rows();
	}
	return s;
}

shared_ptr<SignalFunctor> parseSPGR(const Nifti &img, const bool prompt, const Components nC) {
	double inTR = 0., inTrf = 0., inTE = 0.;
	ArrayXd inAngles(img.dim(4));
	#ifdef AGILENT
	Agilent::ProcPar pp;
	if (ReadPP(img, pp)) {
		inTR = pp.realValue("tr");
		inAngles = pp.realValues("flip1");
		#ifdef USE_MCFINITE
		inTE = pp.realValue("te");
		inTrf = pp.realValue("p1") / 1.e6; // p1 is in microseconds
		#endif
	} else
	#endif
	{
		if (prompt) cout << "Enter TR (seconds): " << flush; cin >> inTR;
		if (prompt) cout << "Enter " << inAngles.size() << " Flip-angles (degrees): " << flush;
		for (int i = 0; i < inAngles.size(); i++) cin >> inAngles[i];
		#ifdef USE_MCFINITE
		if (prompt) cout << "Enter TE (seconds): " << flush; cin >> inTE;
		if (prompt) cout << "Enter RF Pulse Length (seconds): " << flush; cin >> inTrf;
		#endif
		string temp; getline(cin, temp); // Just to eat the newline
	}
	shared_ptr<SignalFunctor> f = make_shared<SPGR_Functor>(inAngles * M_PI / 180., inTR, nC);
	return f;
}

shared_ptr<SignalFunctor> parseSSFP(const Nifti &img, const bool prompt, const Components nC) {
	double inTR = 0., inTrf = 0., inPhase = 0.;
	ArrayXd inPhases, inAngles;
	#ifdef AGILENT
	Agilent::ProcPar pp;
	if (ReadPP(img, pp)) {
		inPhases = pp.realValues("rfphase");
		inTR = pp.realValue("tr");
		inAngles = pp.realValues("flip1");
		#ifdef USE_MCFINITE
		inTrf = pp.realValue("p1") / 1.e6; // p1 is in microseconds
		#endif
	} else
	#endif
	{
		size_t nPhases = 0;
		if (prompt) cout << "Enter number of phase-cycling patterns: " << flush; cin >> nPhases;
		inPhases.resize(nPhases);
		inAngles.resize(img.dim(4) / nPhases);
		if (prompt) cout << "Enter " << nPhases << " phase-cycles (degrees): " << flush;
		for (size_t i = 0; i < nPhases; i++) cin >> inPhases(i);
		if (prompt) cout << "Enter TR (seconds): " << flush; cin >> inTR;
		if (prompt) cout << "Enter " << inAngles.size() << " Flip-angles (degrees): " << flush;
		for (ArrayXd::Index i = 0; i < inAngles.size(); i++) cin >> inAngles(i);
		#ifdef USE_MCFINITE
		if (prompt) cout << "Enter RF Pulse Length (seconds): " << flush; cin >> inTrf;
		#endif
		string temp; getline(cin, temp); // Just to eat the newline
	}
	shared_ptr<SignalFunctor> f = make_shared<SSFP_Functor>(inAngles * M_PI / 180., inTR, inPhases * M_PI / 180., nC);
	return f;
}