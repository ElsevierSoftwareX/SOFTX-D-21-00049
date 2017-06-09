function sLib = libaudiomeasure()
% LIBAUDIOMEASURE - function handle library
%
% Usage:
% libaudiomeasure()
%
% Help to function "fun" can be accessed by calling
% audiomeasure.help.fun()
%

% This file was generated by "bake_mlib audiomeasure".
% Do not edit! Edit sources audiomeasure_*.m instead.
%
% Date: 09-Jun-2017 14:42:43
sLib = struct;
sLib.measure_coh = @audiomeasure_measure_coh;
sLib.help.measure_coh = @help_measure_coh;
sLib.measure_dau2010 = @audiomeasure_measure_dau2010;
sLib.help.measure_dau2010 = @help_measure_dau2010;
sLib.measure_degdiff = @audiomeasure_measure_degdiff;
sLib.help.measure_degdiff = @help_measure_degdiff;
sLib.measure_delay = @audiomeasure_measure_delay;
sLib.help.measure_delay = @help_measure_delay;
sLib.measure_pemoq = @audiomeasure_measure_pemoq;
sLib.help.measure_pemoq = @help_measure_pemoq;
sLib.measure_segmentalsnr = @audiomeasure_measure_segmentalsnr;
sLib.help.measure_segmentalsnr = @help_measure_segmentalsnr;
sLib.measure_snr = @audiomeasure_measure_snr;
sLib.help.measure_snr = @help_measure_snr;
assignin('caller','audiomeasure',sLib);


function c = audiomeasure_measure_coh( s1, s2, N )
  s1 = buffer(s1,N);
  s2 = buffer(s2,N);
  s1 = fft(s1,[],2);
  s2 = fft(s2,[],2);
  Z = s1.*conj(s2);
  Z = Z ./ abs(Z);
  c = mean(abs(mean(Z,2)));

function help_measure_coh
disp([' MEASURE_COH - ',char(10),'',char(10),' Usage:',char(10),'  audiomeasure = libaudiomeasure();',char(10),'  c = audiomeasure.measure_coh( s1, s2, N );',char(10),'']);


function q = audiomeasure_measure_dau2010( SNproc, Sref )
  % measure after Christiansen et al. (2010)
  % requires medi-modules 'gammatone_filterbank' and 'adapt_loop'
  Pref = pemo( Sref, 20480 );
  Pproc = pemo( SNproc, 20480 );
  RMS_tot = 10*log10(mean(Sref(:).^2));
  Nshift = 205;
  Nblocks = floor(size(Sref,1)/Nshift)-1;
  vRMS = zeros(Nblocks,1);
  vCorr = zeros(Nblocks,1);
  for k=1:Nblocks
    idx = (k-1)*Nshift+[1:2*Nshift];
    vRMS(k) = 10*log10(mean(Sref(idx).^2))-RMS_tot;
    vCorr(k) = corr(mean(Pref(idx,:))',mean(Pproc(idx,:))');
  end
  idx_low = find((vRMS<=-5) & (vRMS>=-15));
  idx_mid = find((vRMS<=0) & (vRMS>-5));
  idx_high = find((vRMS>0));
  r_low = mean(vCorr(idx_low));
  r_mid = mean(vCorr(idx_mid));
  r_high = mean(vCorr(idx_high));
  % model parameters:
  w_low = 0;
  w_mid = 0;
  w_high = 1;
  S = 0.056;
  O = 0.39;
  c = w_low*r_low + w_mid*r_mid + w_high*r_high;
  q = 1./(1+exp((O-c)/S));
  
function Y = pemo( x, fs )
  analyzer = Gfb_Analyzer_new(fs,100,1000,8000,1);
  Y = real(Gfb_Analyzer_fprocess(analyzer, x')');
  for k=1:size(Y,2)
    Y(:,k) = haircell(Y(:,k), fs );
    Y(:,k) = adapt_m( Y(:,k), fs );
  end

  

function help_measure_dau2010
disp([' MEASURE_DAU2010 - measure after Christiansen et al. (2010)',char(10),'',char(10),' Usage:',char(10),'  audiomeasure = libaudiomeasure();',char(10),'  q = audiomeasure.measure_dau2010( SNproc, Sref );',char(10),'',char(10),' requires medi-modules ''gammatone_filterbank'' and ''adapt_loop''',char(10),'']);


function dd = audiomeasure_measure_degdiff( x, fs, fftlen, wndlen )
% degree of diffusiveness after Wittkop (2001)
%
% Usage:
% dd = measure_degdiff( x, fs, fftlen, wndlen )
%
% x : input signal (Nx2)
% fs : sampling rate / Hz (orig: 25000)
% fftlen : fft length (orig: 512)
% wndlen : window length (orig: 400)
% dd : degree of diffusiveness
%
% time constants:
% t_MSC = 0.04
% t_dd = signal length (orig: 5s)
  vF = ([1:floor(fftlen/2-1)]'-1)/fftlen*fs;
  Xl = stft(x(:,1),fftlen,wndlen);
  Xr = stft(x(:,2),fftlen,wndlen);
  frate = fs/(0.5*wndlen);
  [B,A] = butter(1,0.04/(0.5*frate));
  MSC = abs(filter(B,A,Xl.*conj(Xr))).^2./max(1e-10,(filter(B,A,abs(Xl).^2) .* filter(B,A,abs(Xr).^2)));
  w_f = ones(size(MSC));
  w_f(vF<1000,:) = 0;
  dd = mean(mean(w_f .* (1-sqrt(MSC)),1));
  
  
function X = stft( x, fftlen, wndlen )
  zpadlen = fftlen-wndlen;
  zpad1 = floor(zpadlen/2);
  zpad2 = zpadlen - zpad1;
  xb = buffer(x, wndlen, wndlen/2 );
  xb = [zeros(zpad1,size(xb,2));...
	xb .* repmat(hann(wndlen),[1,size(xb,2)]);...
	zeros(zpad2,size(xb,2))];
  X = realfft(xb);


function help_measure_degdiff
disp([' MEASURE_DEGDIFF - degree of diffusiveness after Wittkop (2001)',char(10),'',char(10),' Usage:',char(10),'  audiomeasure = libaudiomeasure();',char(10),'  dd = audiomeasure.measure_degdiff( x, fs, fftlen, wndlen );',char(10),'',char(10),'',char(10),' Usage:',char(10),' dd = measure_degdiff( x, fs, fftlen, wndlen )',char(10),'',char(10),' x : input signal (Nx2)',char(10),' fs : sampling rate / Hz (orig: 25000)',char(10),' fftlen : fft length (orig: 512)',char(10),' wndlen : window length (orig: 400)',char(10),' dd : degree of diffusiveness',char(10),'',char(10),' time constants:',char(10),' t_MSC = 0.04',char(10),' t_dd = signal length (orig: 5s)',char(10),'']);


function d = audiomeasure_measure_delay( x, y )
  [c,l] = xcorr(x,y,2048);
  [tmp,idx] = max(c);
  d = l(idx);

function help_measure_delay
disp([' MEASURE_DELAY - ',char(10),'',char(10),' Usage:',char(10),'  audiomeasure = libaudiomeasure();',char(10),'  d = audiomeasure.measure_delay( x, y );',char(10),'']);


function [PSM, PSMt, ODG, PSM_inst] = audiomeasure_measure_pemoq(RefSig, TestSig, fs,varargin )
  [PSM, PSMt, ODG, PSM_inst] = ...
      audioqual(RefSig, TestSig, fs, varargin{:} );

function help_measure_pemoq
disp([' MEASURE_PEMOQ - ',char(10),'',char(10),' Usage:',char(10),'  audiomeasure = libaudiomeasure();',char(10),'  [PSM, PSMt, ODG, PSM_inst] = audiomeasure.measure_pemoq(RefSig, TestSig, fs,varargin );',char(10),'']);


function snr = audiomeasure_measure_segmentalsnr( s, n, fs )
  ;
  
  % 125 ms blocks:
  Nchunk = round(fs*0.125);
  
  snr = [];
  for k=1:size(s,2)
    Sbuf = buffer(s(:,k),Nchunk);
    Nbuf = buffer(n(:,k),Nchunk);
    % short time SNR:
    st_SNR = 10*log10(mean(Sbuf.^2)./max(eps,mean(Nbuf.^2)));
    st_SNR = min(35,max(-20,st_SNR));
    snr(k) = mean(st_SNR);
  end


function help_measure_segmentalsnr
disp([' MEASURE_SEGMENTALSNR - 125 ms blocks:',char(10),'',char(10),' Usage:',char(10),'  audiomeasure = libaudiomeasure();',char(10),'  snr = audiomeasure.measure_segmentalsnr( s, n, fs );',char(10),'',char(10),'']);


function snr = audiomeasure_measure_snr( s, n )
  snr = 10*log10(mean(s.^2)./mean(n.^2));

function help_measure_snr
disp([' MEASURE_SNR - ',char(10),'',char(10),' Usage:',char(10),'  audiomeasure = libaudiomeasure();',char(10),'  snr = audiomeasure.measure_snr( s, n );',char(10),'']);


