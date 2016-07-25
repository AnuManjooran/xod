import R from 'ramda';
import { createSelector } from 'reselect';

import * as PIN_DIRECTION from '../constants/pinDirection';
import * as PIN_VALIDITY from '../constants/pinValidity';
import { LINK_ERRORS } from '../constants/errorMessages';

import { getCurrentPatchId } from './editor';

/*
  Project selectors
*/

const getProjectState = (state, path) => {
  if (path.length > 0 && R.has(path[0], state)) {
    return getProjectState(
      R.prop(path.shift(), state),
      path
    );
  }
  return state;
};

export const getProject = (state) => {
  const path = ['project', 'present'];
  return getProjectState(state, path);
};

export const getJSON = (state) => JSON.stringify(getProject(state));

export const getMeta = R.pipe(
  getProject,
  R.prop('meta')
);

/*
  NodeType selectors
*/

export const getNodeTypes = R.pipe(
  getProject,
  R.prop('nodeTypes')
);

/*
  Node selectors
*/

export const getNodes = R.pipe(
  getProject,
  R.prop('nodes')
);

export const getLastNodeId = (state) => R.pipe(
  getNodes,
  R.values,
  R.map(node => parseInt(node.id, 10)),
  R.reduce(R.max, 0)
)(state);

export const getNodeById = (state, props) => R.pipe(
  getNodes,
  R.filter((node) => node.id === props.id),
  R.values,
  R.head
)(state, props);

export const getPreparedNodes = (state) => getNodes(state);

/*
  Pin selectors
*/

export const getPins = R.pipe(
  getProject,
  R.prop('pins')
);

export const getPinsByNodeId = (state, props) => R.pipe(
  getPins,
  R.filter((pin) => pin.nodeId === props.id)
)(state, props);

export const getPinsByIds = (state, props) => R.pipe(
  getPins,
  R.values,
  R.reduce((p, pin) => {
    let result = p;
    if (props && props.pins && props.pins.indexOf(pin.id) !== -1) {
      result = R.assic(pin.id, pin, p);
    }
    return result;
  }, {})
)(state, props);

export const getFullPinsData = createSelector(
  [getPins, getNodeTypes, getNodes],
  (pins, nodeTypes, nodes) => R.pipe(
    R.values,
    R.reduce((p, pin) => {
      const node = nodes[pin.nodeId];
      const nodeTypePins = nodeTypes[node.typeId].pins;
      const originalPin = nodeTypePins[pin.key];
      return R.assoc(pin.id, R.merge(pin, originalPin), p);
    }, {})
  )(pins)
);

export const doesPinHaveLinks = (pin, links) => R.pipe(
  R.values,
  R.filter((link) => (link.pins[0] === pin.id || link.pins[1] === pin.id)),
  R.length,
  R.flip(R.gt)(0)
)(links);

export const canPinHaveMoreLinks = (pin, links) => (
  (
    pin.direction === PIN_DIRECTION.INPUT &&
    !doesPinHaveLinks(pin, links)
  ) ||
  pin.direction === PIN_DIRECTION.OUTPUT
);

export const getValidPins = (pins, links, forPinId) => {
  const oPin = pins[forPinId];
  return R.pipe(
    R.values,
    R.reduce((p, pin) => {
      const samePin = (pin.id === oPin.id);
      const sameNode = (pin.nodeId === oPin.nodeId);
      const sameDirection = (pin.direction === oPin.direction);
      const sameType = (pin.type === oPin.type);
      const canHaveLink = canPinHaveMoreLinks(pin, links);

      let validness = PIN_VALIDITY.INVALID;


      if (!samePin && !sameNode && canHaveLink) {
        if (!sameDirection) { validness = PIN_VALIDITY.ALMOST; }
        if (!sameDirection && sameType) { validness = PIN_VALIDITY.VALID; }
      }

      const result = {
        id: pin.id,
        validness,
      };

      return R.assoc(pin.id, result, p);
    }, {})
  )(pins);
};

/*
  Link selectors
*/

export const getLinks = R.pipe(
  getProject,
  R.prop('links')
);

export const getLinkById = (state, props) => R.pipe(
  getLinks,
  R.filter((link) => link.id === props.id),
  R.values,
  R.head
)(state, props);

export const getLinksByPinId = (state, props) => R.pipe(
  getLinks,
  R.filter(
    (link) => (
      props.pinIds.indexOf(link.pins[0]) !== -1 ||
      props.pinIds.indexOf(link.pins[1]) !== -1
    )
  ),
  R.values
)(state, props);

export const validateLink = (state, pinIds) => {
  const pins = getFullPinsData(state);
  const linksState = getLinks(state);
  const fromPin = pins[pinIds[0]];
  const toPin = pins[pinIds[1]];

  const sameDirection = fromPin.direction === toPin.direction;
  const sameNode = fromPin.nodeId === toPin.nodeId;
  const fromPinCanHaveMoreLinks = canPinHaveMoreLinks(fromPin, linksState);
  const toPinCanHaveMoreLinks = canPinHaveMoreLinks(toPin, linksState);

  const check = (
    !sameDirection &&
    !sameNode &&
    fromPinCanHaveMoreLinks &&
    toPinCanHaveMoreLinks
  );

  const result = {
    isValid: check,
    message: 'Unknown error',
  };

  if (!check) {
    if (sameDirection) {
      result.message = LINK_ERRORS.SAME_DIRECTION;
    } else
    if (sameNode) {
      result.message = LINK_ERRORS.SAME_NODE;
    } else
    if (!fromPinCanHaveMoreLinks || !toPinCanHaveMoreLinks) {
      result.message = LINK_ERRORS.ONE_LINK_FOR_INPUT_PIN;
    }
  }

  return result;
};

/*
  Patch selectors
*/

export const getPatches = R.pipe(
  getProject,
  R.prop('patches')
);
export const getCurrentPatch = (state) => {
  const curPatchId = getCurrentPatchId(state);
  return R.pipe(
    getPatches,
    R.prop(curPatchId)
  )(state);
};

export const getPatchName = createSelector(
  getCurrentPatch,
  (patch) => R.prop('name')(patch)
);
